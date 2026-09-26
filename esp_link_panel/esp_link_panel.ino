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
 *               beep bands, blink periods, thresholds, persisted buzzer
 *               mute) with virtual board LEDs (real blinking), a buzzer
 *               icon with a mute cross, per-bit fault LEDs and editable
 *               fields; the wire length field grows to u16 (frame = AA 55
 *               type len_lo len_hi payload xor) and PARAMS_BULK to 386
 *               payload bytes (77 params) - both boards must flash
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
 *               (کارت دیوتی دستی از ۱.۱۰ در تب پنل؛ سقف دیوتی، نمونه‌های نمودار، حذف صبر و مقسم پک هم در ۱.۱۰):
 *               ۱) پنل: ولتاژهای مشترک (با کالیبراسیون آفست از مولتی‌متر)، فیلتر جریان (مدین ۱..۱۵،
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

/* ==================== Link Constants ==================== */
#define ESP_LINK_BAUD_RATE          921600u
#define ESP_LINK_RX_BUFFER_SIZE     1024u
#define ESP_LINK_SOF_BYTE0          0xAAu
#define ESP_LINK_SOF_BYTE1          0x55u
#define ESP_LINK_HEADER_SIZE        5u   /* SOF0 + SOF1 + type + len_lo + len_hi (v1.16 u16 length) */
/* [EN] 512 since v1.16 (user order 2026-09-26): PARAMS_BULK with 77
         parameters = 1 + 77 x 5 = 386 payload bytes (was 191 for 38 in
         v1.15). The length field is u16 little-endian. Both boards MUST
         flash together.
         / [FA] از v1.16 (دستور کاربر ۲۰۲۶-۰۹-۲۶): PARAMS_BULK با ۷۷ پارامتر
         = ۱ + ۷۷ × ۵ = ۳۸۶ بایت payload (قبلاً ۱۹۱ برای ۳۸ در v1.15).
         فیلد طول u16 لیتل‌اندین است. هر دو برد باید با هم فلش شوند. */
#define ESP_LINK_MAX_PAYLOAD        512u
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
#define ESP_MSG_TLM_LIVE            0x10u
#define ESP_MSG_PARAM_REPORT        0x11u
#define ESP_MSG_PARAMS_BULK         0x12u

/* ==================== Parameter Constants ==================== */
/* [EN] 77 since v1.16 (user order 2026-09-26): ids 20..26 = the shared
         charge profile (section 5.7), ids 27..37 = the alarms tab (27..34
         fault supervision, 35..37 charger safety ceilings), ids 38..76 =
         UI cadence (LED/beep patterns, bands, blink, thresholds, mute).
         / [FA] از v1.16: شناسه‌های ۲۰..۲۶ = پروفایل شارژ مشترک (بخش 5.7)،
         شناسه‌های ۲۷..۳۷ = تب آلارم‌ها (۲۷..۳۴ نظارت فالت، ۳۵..۳۷ سقف‌های
         ایمنی شارژر)، شناسه‌های ۳۸..۷۶ = اعداد UI (الگوهای LED/بوق، باندها،
         چشمک، آستانه‌ها، میوت). */
#define ESP_PARAM_COUNT             77u
#define ESP_PARAM_CHG1_ENABLE       11u
#define ESP_PARAM_CHG2_ENABLE       12u
#define ESP_PARAM_MANUAL_TEST_MODE  19u
#define ESP_TLM_FLAG_MANUAL_MODE    0x20u

/* ==================== CAL_REFERENCE (removed from the panel in v1.7) ==================== */
/* [EN] The v1.6 CAL card was dropped in the v1.7 simplification (user order 2026-09-25): the panel
        no longer sends CAL_REFERENCE. The STM32 handler stays in the firmware, unused and harmless.
   [FA] کارت CAL در ساده‌سازی نسخهٔ ۱.۷ حذف شد (دستور کاربر ۲۰۲۶-۰۹-۲۵): پنل دیگر CAL_REFERENCE
        نمی‌فرستد. handler آن در فریم‌ور STM32 مانده است؛ بی‌استفاده و بی‌ضرر. */

/* ==================== Bench Statistics Window (spec 5.5 / 5.6 v2) ==================== */
/* [EN] The browser polls /t only every 300 ms, so the bench tools read EVERY TLM frame (10 Hz) through
        this window: POST /m restarts it (and queues one GET_PARAMS so the logged parameters are the live
        ones), GET /m reads it. All 20 t[] fields are tracked (ch1 0..6, ch2 7..13, Vin 14, V24 15, V12 16,
        Vlow 17, Vhigh 18, faults 19): sum / min / max / last frame; faults are also ORed over the window;
        seq and flags are the last frame's. The count stops at 60000 frames (100 min) so u32 sums never wrap.
   [FA] مرورگر فقط هر ۳۰۰ms /t را می‌خواند؛ پس ابزارهای بنچ تک‌تک فریم‌های TLM (۱۰ هرتز) را از این پنجره
        می‌گیرند: POST /m آن را از نو شروع می‌کند (و یک GET_PARAMS صف می‌کند تا پارامترهای ثبت‌شده همان مقادیر
        زنده باشند) و GET /m آن را می‌خواند. هر ۲۰ فیلد t[] دنبال می‌شود (کانال ۱ 0..6، کانال ۲ 7..13، Vin 14،
        V24 15، V12 16، Vlow 17، Vhigh 18، خطاها 19): مجموع / کمینه / بیشینه / آخرین فریم؛ خطاها در کل پنجره
        OR هم می‌شوند؛ seq و flags مال آخرین فریم‌اند. شمارش در ۶۰۰۰۰ فریم (۱۰۰ دقیقه) می‌ایستد تا مجموع u32 سرریز نشود. */
#define ESP_STAT_FAULT_FIELD        19u
#define ESP_STAT_MAX_FRAMES         60000u

/* ==================== Bench Data Log File (spec 5.6, CSV v2 + DMM v4: 128 self-contained columns) ==================== */
/* [EN] One append-only CSV on LittleFS. The panel builds each row from the /m window (every TLM frame,
        raw included) plus the typed DMM readings and POSTs it to /benchlog/add; the ESP only validates
        (printable ASCII, newline-terminated, bounded length) and appends. The column header (the comment block
        of spec 5.6, 128 columns - v1.12: +7 charge-profile params, v1.15: +11 alarm params, v1.16: +39 UI cadence params) is written by the ESP when the file is created. Appending stops at the cap (HTTP 507) and the UI warns.
        Arduino IDE: pick a flash layout WITH a file system (ESP8266 e.g. "4MB (FS:1MB)"; ESP32 default is fine).
   [FA] یک فایل CSV فقط-افزودنی روی LittleFS. پنل هر ردیف را از پنجرهٔ /m (تک‌تک فریم‌های TLM با raw)
        و عددهای مولتی‌متر می‌سازد و به /benchlog/add می‌فرستد؛ ESP فقط بررسی (ASCII قابل چاپ، پایان با
        خط جدید، طول محدود) و اضافه می‌کند. بلوک عنوان ستون‌ها (بلوک توضیح بخش 5.6، ۱۲۸ ستون - v1.12: +۷ پارامتر پروفایل شارژ، v1.15: +۱۱ پارامتر آلارم، v1.16: +۳۹ پارامتر UI) را ESP هنگام ساخت فایل می‌نویسد. در سقف
        اندازه افزودن متوقف می‌شود (HTTP 507) و پنل هشدار می‌دهد.
        در Arduino IDE چیدمان فلشِ دارای فایل‌سیستم را انتخاب کنید (ESP8266 مثلاً "4MB (FS:1MB)"؛ ESP32 پیش‌فرض کافی است). */
#define ESP_BENCHLOG_PATH           "/benchlog.csv"
#define ESP_BENCHLOG_MAX_BYTES      102400u
#define ESP_BENCHLOG_MAX_POST       1536u
#define ESP_BENCHLOG_HEADER \
    "# cols:\n" \
    "#  [id]     scenario,step,duty_permille,settle_ms,sample_ms,browser_ts\n" \
    "#  [params] off1,off2,gain1,gain2,voff_in,voff_24,voff_12,med,avg,\n" \
    "#           eta1,eta2,en1,en2,ceil1,ceil2,fixon1,fix1,fixon2,fix2,manual\n" \
    "#  [profile] chg_absorb_mv,chg_absorb_enter_mv,chg_absorb_over_mv,\n" \
    "#            chg_float_mv,chg_reentry_mv,chg_bulk_imax_ma,chg_taper_ma\n" \
    "#  [alarms]  alm_disc_mv,alm_disc_deb_ms,alm_absent_mv,alm_back_mv,\n" \
    "#            alm_absent_deb_ms,alm_recover_ms,alm_in_min_mv,alm_in_max_mv,\n" \
    "#            alm_hard_ma,alm_ov_mv,alm_floor_mv\n" \
    "#  [uicad]   ui_ov_led_per,ui_ov_led_duty,ui_ov_beep_per,ui_ov_beep_dur,\n" \
    "#            ui_ov_beep_cnt,ui_ov_beep_gap,ui_bl_led_per,ui_bl_led_duty,\n" \
    "#            ui_bl_beep_per,ui_bl_beep_dur,ui_bl_beep_cnt,ui_bl_beep_gap,\n" \
    "#            ui_run_start,ui_run_double,ui_run_triple,ui_run_crit,\n" \
    "#            ui_run_std_per,ui_run_tri_per,ui_run_crit_per,ui_run_crit_duty,\n" \
    "#            ui_run_crit_cnt,ui_run_std_dur,ui_run_tri_dur,ui_run_crit_dur,\n" \
    "#            ui_run_std_cnt,ui_run_dbl_cnt,ui_run_tri_cnt,ui_run_gap,\n" \
    "#            ui_green_per,ui_green_min,ui_yellow_per,ui_yellow_min,\n" \
    "#            ui_ov_thr,ui_ov_hyst,ui_lowbat_thr,ui_lowbat_clr,\n" \
    "#            ui_pct_vmin,ui_pct_vmax,ui_mute\n" \
    "#  [ch1]    raw1,raw1_min,raw1_max,shunt1_uv,unf1,unf1_min,unf1_max,\n" \
    "#           filt1,filt1_min,filt1_max,iest1,iest1_min,iest1_max,duty1,state1\n" \
    "#  [ch2]    raw2,raw2_min,raw2_max,shunt2_uv,unf2,unf2_min,unf2_max,\n" \
    "#           filt2,filt2_min,filt2_max,iest2,iest2_min,iest2_max,duty2,state2\n" \
    "#  [glob]   seq,flags,vin_mv,v24_mv,v12_mv,vlow_mv,vhigh_mv,faults_or\n" \
    "#  [dmm]    dmm_i_in_ma,dmm_vin_mv,dmm_i_bat1_ma,dmm_vbat1_mv,\n" \
    "#           dmm_i_bat2_ma,dmm_vbat2_mv,note\n" \
    "# run <n> browser_ts=<ISO from the panel page> scenario=<SOLO1|SOLO2|BOTH>\n" \
    "#  duty_list=<...>\n"

/* ==================== Wi-Fi / HTTP Constants ==================== */
#define ESP_WIFI_AP_SSID            "ChangeOver-ESP"
#define ESP_WIFI_AP_PASS            "123456789"
#define ESP_HTTP_PORT               80
#define ESP_JSON_BUFFER_SIZE        2048u   /* v1.16: p[77] needs the headroom (~950 B worst case) */
#define ESP_HTTP_FONT_CACHE         "public, max-age=31536000"

/* ==================== Parser States ==================== */
typedef enum
{
    ESP_RX_WAIT_SOF0 = 0,
    ESP_RX_WAIT_SOF1,
    ESP_RX_WAIT_TYPE,
    ESP_RX_WAIT_LEN_LO,
    ESP_RX_WAIT_LEN_HI,
    ESP_RX_WAIT_PAYLOAD,
    ESP_RX_WAIT_XOR
} esp_rx_state_t;

/* ==================== Parameter Ranges (STM32 clamps too) ==================== */
/* [EN] ID: 0..1 offset, 2..3 gain, 4..6 mV offset (signed), 7 median 1..15, 8 avg window 1..300 (v1.4, raised in v1.9), 9..10 ETA conversion (v1.3, 0 = identity),
        11..12 charger enable, 13..14 duty ceiling, 15/17 fixed-duty on, 16/18 fixed/manual duty,
        19 manual test mode (v1.2), 20..26 charge profile (v1.12: outer envelope only - the STM32
        re-clamps the interdependencies, e.g. enter <= absorb-50), 27..37 alarms tab (v1.15:
        outer envelope only - the STM32 re-clamps the set, e.g. 27 in over+50..OV-100,
        35 never above 950).
   [FA] شناسه: ۰..۱ آفست، ۲..۳ گین، ۴..۶ آفست mV علامت‌دار، ۷ مدین ۱..۱۵، ۸ پنجره میانگین ۱..۳۰۰ (نسخه ۱.۴؛ بالا رفتن در ۱.۹)، ۹..۱۰ ضریب تبدیل η (v1.3، صفر = همانی)،
        ۱۱..۱۲ قطع/وصل شارژر، ۱۳..۱۴ سقف duty، ۱۵/۱۷ مود duty فیکس، ۱۶/۱۸ duty فیکس/دستی،
        ۱۹ مود تست دستی (نسخه ۱.۲)، ۲۷..۳۷ تب آلارم‌ها (نسخه ۱.۱۵: فقط پاکت بیرونی —
        برد مجموعه را دوباره گیره می‌زند)، ۳۸..۷۶ اعداد UI (نسخه ۱.۱۶: فقط پاکت بیرونی). */
static const int32_t INT32_T__G__ParamMin[ESP_PARAM_COUNT] = {   0,   0,  100,  100, -5000, -5000, -5000, 1,  1,   0,   0, 0, 0,   0,   0, 0,   0, 0,   0, 0, 11000, 10500, 11100, 9000, 8000, 100,  10, 14000,  50, 3000, 4000,  100,  100, 18000, 24000,  150, 14000,     0,   100,     0,     0,     0,   0,     0,   100,     0,     0,     0,   0,     0,     0,     0,     0,     0,     0,     0,     0,     0,   0,     0,     0,     0,   0,   0,   0,     0,   100,     0,   100,     0, 24000,     0, 15000, 15000, 15000, 25000,     0 };
static const int32_t INT32_T__G__ParamMax[ESP_PARAM_COUNT] = { 255, 255, 3000, 3000,  5000,  5000,  5000, 15, 300, 999, 999, 1, 1, 500, 500, 1, 500, 1, 500, 1, 14600, 14550, 14750, 14300, 13200, 900, 300, 15000, 1000, 8000, 9000, 5000, 5000, 24000, 30000,  950, 15000,  8000, 10000,   100, 600000, 600000,  10,  5000, 10000,   100, 600000, 600000,  10,  5000,   100,   100,   100,   100, 600000, 600000, 600000,   100,  10, 600000, 600000, 120000,  10,  10,  10,  5000, 10000, 10000, 10000, 10000, 32000,  2000, 24000, 24000, 25000, 32000,     1 };

/* ==================== RX State ==================== */
static esp_rx_state_t ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
static uint8_t  UINT8_T__G__RxType = 0u;
static uint16_t UINT16_T__G__RxLen = 0u;
static uint16_t UINT16_T__G__RxIndex = 0u;
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

/* [EN] Bench statistics window (spec 5.6 v2) / [FA] پنجرهٔ آمار بنچ (بخش 5.6 نسخه ۲) */
static bool     BOOL__G__FsOk = false;   /* [EN] LittleFS mounted / [FA] LittleFS سوار شده */
static uint32_t UINT32_T__G__StatSum[ESP_LINK_TLM_FIELD_COUNT];
static uint32_t UINT32_T__G__StatMin[ESP_LINK_TLM_FIELD_COUNT];
static uint32_t UINT32_T__G__StatMax[ESP_LINK_TLM_FIELD_COUNT];
static uint32_t UINT32_T__G__StatLast[ESP_LINK_TLM_FIELD_COUNT];
static uint32_t UINT32_T__G__StatFaultOr = 0u;
static uint16_t UINT16_T__G__StatSeq = 0u;
static uint8_t  UINT8_T__G__StatFlags = 0u;
static uint32_t UINT32_T__G__StatCount = 0u;

/* [EN] Send priority: charger cut, manual mode, manual duties, then the rest.
   [FA] اولویت ارسال: قطع شارژر، مود دستی، duty دستی، سپس بقیه. */
static const uint8_t UINT8_T__G__TxOrder[ESP_PARAM_COUNT] = { 11, 12, 19, 16, 18, 15, 17, 13, 14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76 };

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
.ms{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin-top:10px;text-align:center}.ms div{background:#0c1018;border-radius:8px;padding:6px 2px}.ms b{display:block}
.off{background:#5b1c26;font-size:16px}
.fx{direction:ltr;text-align:right;unicode-bidi:isolate;font-size:11px;color:#6f7a93;font-variant-numeric:tabular-nums;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.fb{background:#0c1018;border-radius:10px;padding:8px 10px;margin-bottom:6px}.fb .fx{font-size:12px;color:#9aa6c0;line-height:1.9;white-space:normal}.fb .lb{font-size:11px}
.as{display:flex;flex-wrap:wrap;align-items:center;gap:6px 10px;padding:9px 0;border-top:1px solid var(--ln)}.as .nm{flex:1 1 180px}.as .lv{font-weight:600;margin-left:4px}
.sb2{background:#243052;color:#cfe0ff}.qr{background:#243052;color:#cfe0ff}.qr.j{background:#b8323f;color:#fff}.qr:disabled{opacity:.4;cursor:default}.wt tr.wa td{background:#3a3212}.wt .wi{width:76px}
canvas{width:100%;height:140px;display:block;background:#0c1018;border-radius:10px;margin-top:10px;direction:ltr}
.lg{display:flex;gap:14px;flex-wrap:wrap;font-size:12px;color:var(--mu);margin-top:6px}.lg i{display:inline-block;width:12px;height:3px;border-radius:2px;margin-left:5px;vertical-align:middle}
.ti2{display:flex;justify-content:space-between;align-items:center}
.ca{border-top:1px solid var(--ln);margin-top:4px;padding-top:12px}.ca .ti{margin-bottom:4px;color:var(--tx)}
.cg{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px}
.cb{border:0;border-radius:10px;padding:8px 10px;background:#243052;color:#cfe0ff;cursor:pointer;display:flex;justify-content:space-between;align-items:center}.cb b{font-weight:500;color:#8fb3ff}
.cb.lo{background:#1c2130;color:var(--mu)}.cb:disabled{opacity:.45;cursor:default}
.cm{margin-top:10px;min-height:1.6em}.cm.g{color:var(--ok)}.cm.r{color:var(--er)}.wr{color:var(--wa)}
body.dn #sh,body.dn #ch{opacity:.45;filter:grayscale(1)}
.bsb{position:sticky;top:58px;z-index:4;border-color:#5a4418}.bqr2{display:flex;gap:8px;margin-top:12px}
.bctl{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin-top:10px}
.tw{overflow:auto;max-height:420px;margin:6px 0 10px}.bt2{font-size:12px;direction:ltr;white-space:nowrap}.bt2 th{position:sticky;top:0;background:var(--cd);color:var(--mu);font-weight:500;text-align:left;padding:4px 6px}.bt2 td{padding:4px 6px;text-align:left}
.bt3{width:auto;font-size:13px}.bt3 th{color:var(--mu);font-weight:500;text-align:right;padding:4px 6px;white-space:nowrap}.bt3 td{padding:4px 6px;text-align:right}.bt3 input[type=number]{padding:4px 6px}
.bsum{font-size:12px;direction:ltr;text-align:left;line-height:1.9;margin-bottom:8px}.okc{color:var(--ok)}.erc{color:var(--er)}
.bxw textarea{width:100%;height:150px;background:#0c1018;color:#9aa6c0;border:1px solid var(--ln);border-radius:10px;padding:8px;font:11px/1.5 monospace;direction:ltr;margin-top:6px}
.ds{font-size:13px;line-height:2;color:#c3cad8;background:#0c1018;border-radius:10px;padding:10px 14px}.ds ul{padding-right:18px}.ds b{color:var(--tx)}
.qs{display:grid;grid-template-columns:1fr 1fr;gap:12px}.q{background:#0c1018;border-radius:10px;padding:10px}.q input[type=number]{width:90px}.q .cut{background:var(--er)}.q .run{background:var(--ok);color:#06140d}
select{font:inherit;color:inherit;background:#0c1018;border:1px solid var(--ln);border-radius:8px;padding:5px 7px}
.eg3{display:flex;flex-wrap:wrap;gap:10px 16px;margin-top:6px}.eg3 label{display:flex;flex-direction:column;gap:3px;font-size:13px}
.pgx{display:none}.pgx.a{display:block}body:not(.br) .wstop{display:none}body.br .brun{opacity:.4;pointer-events:none}a.lnk{text-decoration:none;display:inline-block}
.bq{background:#0c1018;border:1px solid var(--wa);border-radius:10px;padding:10px;margin-top:10px}.bqr{display:flex;flex-wrap:wrap;gap:10px;align-items:flex-end;margin-top:8px}.bqr label{display:flex;flex-direction:column;gap:3px;font-size:13px}.bqr input[type=number]{width:120px}nav{margin-bottom:12px}
.cc .ca{border-top:0;margin-top:0;padding-top:0}.stp2{background:var(--er);white-space:nowrap}
/* صفحهٔ واحد لپ‌تاپ: نوار مشترک بالا، دو ستون شارژر، تست بنچ پایین */
.vt{background:#0c1018;border-radius:12px;padding:10px 8px;display:flex;flex-direction:column;align-items:center;gap:4px}.vt .fx{text-align:center;max-width:100%}
.vc{justify-content:center}.vc input[type=number]{width:96px}
.fl{margin-top:0;padding-top:0;border-top:0}#sh .hd{flex-wrap:wrap;gap:8px}
.sec{display:flex;justify-content:space-between;align-items:center;font-size:13px;font-weight:700;color:var(--mu);margin:14px 0 6px;padding-top:12px;border-top:1px solid var(--ln)}
.frr{display:grid;grid-template-columns:1fr 1fr;gap:0 28px}.frr .rw:first-of-type{border-top:1px solid var(--ln)}.fxw{margin-top:6px;font-size:12px}
.kc{font-size:11px;margin-top:6px}.cr{margin-top:10px;flex-wrap:wrap}.cr .cb{flex:1 1 120px}.cr input[type=number]{width:120px}
.off2{background:#5b1c26;white-space:nowrap}
@media(max-width:1000px){.ch,.frr,.qs{grid-template-columns:1fr}.vs{grid-template-columns:repeat(3,1fr)}}
@media(max-width:640px){.cb{font-size:12px;padding:8px 7px}.cb span{white-space:nowrap}.vs{grid-template-columns:repeat(3,1fr)}.ch{grid-template-columns:1fr}.ms{grid-template-columns:repeat(3,1fr)}}
.sbt{display:flex;gap:4px;background:var(--cd);border:1px solid var(--ln);border-radius:10px;padding:3px;margin-bottom:10px}
.sbt button{flex:1;border:0;background:none;border-radius:7px;padding:6px;cursor:pointer;color:var(--mu);font-weight:600}
.sbt button.a{background:#1f2738;color:var(--tx)}
.sgx{display:none}.sgx.a{display:block}
.ag{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:8px;margin:6px 0}
.ab{background:#0c1018;border:1px solid var(--ln);border-radius:10px;padding:8px 10px;min-height:88px}
.ab small{color:var(--mu)}.ab b{font-size:18px;display:block;margin:1px 0}.ab .lb{display:block;min-height:20px}.ab .tg{margin-top:3px;display:inline-block}
.ab.bad{border-color:#7a2b33}.ab.warn{border-color:#6b5206}.ab.good{border-color:#1d4a3a}
.leds{display:flex;gap:14px;align-items:center;margin:2px 0 10px;flex-wrap:wrap;background:#0c1018;border:1px solid var(--ln);border-radius:10px;padding:8px 12px}.led{display:flex;flex-direction:column;align-items:center;gap:2px;min-width:56px}.led i{width:26px;height:26px;border-radius:50%;background:#2a3245;box-shadow:inset 0 0 4px #000}.led small{color:var(--mu);font-size:11px}.led.r.on i{background:#ff3b3b;box-shadow:0 0 14px #ff3b3b}.led.y.on i{background:#ffd23b;box-shadow:0 0 14px #ffd23b}.led.g.on i{background:#2eff8f;box-shadow:0 0 14px #2eff8f}.bit{display:inline-flex;align-items:center;gap:6px;margin:2px 8px 2px 0}.bit i{width:14px;height:14px;border-radius:50%;background:#2a3245;display:inline-block}.bit.on i{background:#ff3b3b;box-shadow:0 0 8px #ff3b3b}.bz{font-size:30px;line-height:1;position:relative;min-width:44px;text-align:center}.bz.off{opacity:.22;filter:grayscale(1)}.bz .mx{position:absolute;inset:-4px 0 0 0;color:#ff3b3b;font-size:36px;display:none;font-weight:700;text-shadow:0 0 6px #000}.bz.muted .mx{display:block}.bz.muted{opacity:.85}
.leds.stick{position:sticky;top:0;z-index:50;box-shadow:0 2px 14px rgba(0,0,0,.55)}
.fx2{font-size:12px;color:#c3cad8;line-height:1.9;margin-top:4px}
</style></head><body>
<header><h1>پنل ChangeOver</h1><div class="lk" id="lk"><span id="lt">در حال اتصال…</span><i></i></div></header>
<nav><button class="a" data-t="0">پنل</button><button data-t="1">داده‌برداری بنچ</button><button data-t="2">تنظیمات</button></nav>
<div class="wn gb" id="mb"><div class="mx"><div><b>مود تست دستی فعال است</b> — شارژر خودکار و محافظت‌های باتری متوقف‌اند. <span id="ka"></span></div><button class="sb stp2" id="mx">خروج از مود دستی</button></div></div>
<main id="pg">
<div class="pgx a" id="p0">
<div class="cd" id="sh"><div class="hd"><b>ولتاژها <span class="lb">· عدد مولتی‌متر (V) را کنار هر ولتاژ وارد کنید تا آفست آن کالیبره شود</span></b><div class="fl" id="fl"></div></div><div class="vs" id="vs"></div>
<div class="sec">فیلتر جریان <span class="lb">(مشترک هر دو کانال)</span></div><div class="frr" id="fg"></div><div class="fx fxw" id="ff"></div></div>
<div class="ch" id="ch"></div>
<div id="mc"></div>
<div class="cd">
<div class="hd"><b>وضعیت آلارم‌ها</b><span class="lb">· زنده از TLM برد · آستانه‌ها = مقادیر اعمال‌شدهٔ برد</span></div>
<div id="ast" style="display:flex;flex-wrap:wrap;gap:6px;margin:6px 0"></div>
<div id="abars"></div>
</div>
</div>
<div class="pgx" id="p1"></div>
<div class="pgx" id="p2">
<div class="sbt" id="sbt"><button class="a" data-s="0">شارژ و فیلتر</button><button data-s="1">سناریوها</button><button data-s="2">نظارت و ایمنی</button><button data-s="3">پشتیبان‌گیری</button></div>
<div class="sgx a" id="s0">
<div class="cd">
<div class="hd"><b>نمودار مراحل شارژ</b><span class="lb">· مشترک هر دو کانال · ناحیه‌ها از مقادیر اعمال‌شدهٔ برد · تایپ = خط‌چین پیش‌نمایش · ترکیب نامعتبر = هشدار قرمز</span></div>
<div id="qw" style="margin:2px 0 0"></div>
<div id="qg" style="direction:ltr;overflow-x:auto"></div>
<div class="lb" id="qgl">در انتظار دادهٔ برد…</div>
</div>
<div class="cd">
<div class="hd"><b>فیلتر جریان</b><span class="lb">· مشترک هر دو کانال · Median + Average · مثل بقیه روی فلش برد ذخیره می‌شود</span></div>
<div class="bqr">
<label>پنجرهٔ مدین (Median)<input type="number" id="q7" step="1" min="1" max="15"><span class="lb" id="a7">—</span></label>
<label>پنجرهٔ میانگین (Average)<input type="number" id="q8" step="1" min="1" max="300"><span class="lb" id="a8">—</span></label>
</div>
<div class="lb">پنجرهٔ مدین: مرحلهٔ اول فیلتر، هر عدد ۱ تا ۱۵ (زوج هم مجاز)؛ ۱ و ۲ = خاموش، ۳ = پیش‌فرض، بزرگ‌تر = حذف پالس قوی‌تر با تاخیر بیشتر.
پنجرهٔ میانگین: مرحلهٔ دوم، هر عدد ۱ تا ۳۰۰ — میانگین آخرین W خروجی مدین (هر نمونه ۱ms = ۱ms تاریخچه)؛ ۱ = خاموش، ۱۰ = پیش‌فرض. برای صاف‌شدن قابل‌مشاهده روی نمودار تب «پنل» مجموع را بالای ~۲۰۰ms ببرید؛ در مود خودکار شارژر بالای ~۵۰ توصیه نمی‌شود (کندی حلقهٔ تنظیم ۱۰۰Hz).</div>
</div>
<div class="cd">
<div class="hd"><b>پروفایل شارژ</b><span class="lb">· مشترک هر دو کانال · روی فلش برد ذخیره می‌شود و با قطع برق می‌ماند (~۱٫۵ ثانیه پس از آخرین تغییر)</span></div>
<div class="sec">ولتاژها <span class="lb">(mV)</span></div>
<div class="bqr">
<label>حداکثر ولتاژ باتری (ابزورب)<input type="number" id="q20" step="50" min="11000" max="14600"><span class="lb" id="a20">—</span></label>
<label>آستانهٔ ورود به ابزورب<input type="number" id="q21" step="10" min="10500" max="14550"><span class="lb" id="a21">—</span></label>
<label>سقف تجاوز ابزورب<input type="number" id="q22" step="10" min="11100" max="14750"><span class="lb" id="a22">—</span></label>
<label>ولتاژ شناور<input type="number" id="q23" step="50" min="9000" max="14300"><span class="lb" id="a23">—</span></label>
<label>ولتاژ بازگشت به بالک<input type="number" id="q24" step="50" min="8000" max="13200"><span class="lb" id="a24">—</span></label>
</div>
<div class="sec">جریان‌ها <span class="lb">(mA)</span></div>
<div class="bqr">
<label>جریان حداکثر شارژ (بالک)<input type="number" id="q25" step="10" min="100" max="900"><span class="lb" id="a25">—</span></label>
<label>جریان تیپر (ورود به شناور)<input type="number" id="q26" step="5" min="10" max="300"><span class="lb" id="a26">—</span></label>
</div>
<div class="lb">حداکثر ولتاژ باتری: ولتاژ تثبیت فاز ابزورب — بالای آن سوئیچینگ متوقف می‌شود (پیش‌فرض ۱۴۴۰۰).
آستانهٔ ورود: با رسیدن باتری به این ولتاژ فاز ابزورب با پلهٔ ریز ۰٫۱٪ آغاز می‌شود (۱۴۳۰۰).
سقف تجاوز: بالای این ولتاژ کاهش سریع duty (پلهٔ ۰٫۵٪)؛ همیشه ۵۰mV زیر خطای قطع باتری ۱۴٫۸V نگه داشته می‌شود (۱۴۶۰۰).
ولتاژ شناور: نگه‌داشت باتری پس از پایان شارژ (۱۳۵۰۰).
ولتاژ بازگشت: افت باتری در شناور زیر این مقدار، بالک را دوباره آغاز می‌کند (۱۲۸۰۰).
جریان حداکثر: سقف باند تنظیم جریان بالک؛ کف باند به‌طور خودکار ۲۰mA کمتر است (۶۵۰).
جریان تیپر: ابزورب پایان می‌یابد وقتی جریان دنباله ۶۰ ثانیه پایدار زیر این مقدار بماند (۵۰ ~ C/90).
پس از هر تغییر، مقدار «اعمال‌شدهٔ» برد کنار همان فیلد نشان داده می‌شود — اگر با درخواست شما فرق دارد یعنی گیره خورده تا مجموعه سازنده بماند (مثلاً ورود ≤ ابزورب−۵۰). سقف‌های ایمنی (خطای سخت ۹۵۰mA و قطع OV ۱۵V) از تب «آلارم‌ها» فقط پایین‌بردنی‌اند و هرگز بالای مقدار کارخانه نمی‌روند.
ماندگاری: هر پارامتری که از پنل ثبت کنید (~۱٫۵ ثانیه بعد) در فلش خودِ برد ذخیره می‌شود و خاموش/روشن کردن برد آن را از بین نمی‌برد؛ دکمهٔ «بازگردانی پیش‌فرض کارخانه» پیش‌فرض‌ها را می‌فرستد و همان‌ها ذخیره می‌شوند. مودهای تست (دیوتی فیکس/دستی) هرگز ذخیره نمی‌شوند — بعد از هر ریست، شارژر خودکار است.
نگهبان ترکیب: اگر عددهای تایپ‌شده با هم ناسازگار باشند (مثلاً شناور بالای ابزورب−۳۰۰)، بالای نمودار هشدار قرمز می‌آید، فیلد مقصر قرمز می‌شود و قبل از ارسال تأیید گرفته می‌شود — چون برد همان را گیره می‌زند و ناحیه‌ها را به‌هم‌ریخته نمی‌گذارد.</div>
<div class="bqr"><button class="sb sb2" onclick="qdef()">بازگردانی پیش‌فرض کارخانه</button></div>
</div>
</div>
<div class="sgx" id="s1">
<div class="leds stick" id="uleds">
<div class="led r" id="ulR"><i></i><small>قرمز</small></div>
<div class="led y" id="ulY"><i></i><small>زرد</small></div>
<div class="led g" id="ulG"><i></i><small>سبز</small></div>
<div class="bz off" id="ulB">🔊<span class="mx">✕</span></div>
<div style="display:flex;flex-direction:column;gap:2px;flex:1;min-width:220px"><span class="lb" id="uscn">—</span><span class="lb" id="utim">—</span></div>
<button class="sb" onclick="xmute()">🔇/🔊 میوت</button><span class="lb" id="xmuteS">—</span>
</div>
<div class="hd" style="margin-top:10px"><b>سناریوهای LED و بازر</b><span class="lb">· یک سناریو را انتخاب کنید · همه روی فلش برد ذخیره می‌شوند</span></div>
<div id="aw2" style="margin:2px 0 0"></div>
<div class="sbt" id="usel"><button class="a" data-u="1">۱ · اضافه‌ولتاژ</button><button data-u="2">۲ · قطع باتری</button><button data-u="3">۳ · دشارژ</button><button data-u="4">۴ · شارژ عادی</button><button data-u="5">۵ · باتری و درصد</button></div>
<div class="cd" id="ucard1">
<div class="hd"><b>سناریو ۱ — اضافه‌ولتاژ ورودی</b><span class="lb">· سقف ولتاژ + چشمک و بوق · اولویت اول برد</span></div>
<div class="sec">سقف ولتاژ <span class="lb">(mV)</span></div>
<div class="bqr">
<label>آستانه اضافه‌ولتاژ ورودی (mV)<input type="number" id="q70" step="100" min="24000" max="32000"><span class="lb" id="a70">—</span></label>
<label>هیسترزیس اضافه‌ولتاژ (mV)<input type="number" id="q71" step="100" min="0" max="2000"><span class="lb" id="a71">—</span></label>
</div>
<div class="sec">چشمک قرمز <span class="lb">(دوره/دیوتی)</span></div>
<div class="bqr">
<label>دوره چشمک قرمز (ms)<input type="number" id="q38" step="50" min="100" max="10000"><span class="lb" id="a38">—</span></label>
<label>دیوتی قرمز (٪)<input type="number" id="q39" step="5" min="0" max="100"><span class="lb" id="a39">—</span></label>
</div>
<div class="sec">بوق</div>
<div class="bqr">
<label>دوره بوق (ms، صفر=خاموش)<input type="number" id="q40" step="500" min="0" max="600000"><span class="lb" id="a40">—</span></label>
<label>مدت هر بوق (ms)<input type="number" id="q41" step="50" min="0" max="600000"><span class="lb" id="a41">—</span></label>
<label>تعداد بوق<input type="number" id="q42" step="1" min="0" max="10"><span class="lb" id="a42">—</span></label>
<label>گپ بین بوق‌ها (ms)<input type="number" id="q43" step="50" min="0" max="5000"><span class="lb" id="a43">—</span></label>
</div>
<div class="lb">روند: عبور ورودی از سقف ← قرمز چشمک + بوق دوره‌ای (سبز ثابت می‌ماند) ← افت تا سقف−هیسترزیس ← پاک‌شدن و بازگشت به سناریوی قبلی. پیش‌فرض: چشمک ۱۰۰۰/۵۰٪ + یک بوق ۱ثانیه‌ای هر ۱۰ ثانیه.</div>
</div>
<div class="cd" id="ucard2" style="display:none">
<div class="hd"><b>سناریو ۲ — قطع باتری</b><span class="lb">· شناسه‌های ۴۴..۴۹ · اولویت دوم برد</span></div>
<div class="sec">چشمک قرمز <span class="lb">(دوره/دیوتی)</span></div>
<div class="bqr">
<label>دوره چشمک قرمز (ms)<input type="number" id="q44" step="50" min="100" max="10000"><span class="lb" id="a44">—</span></label>
<label>دیوتی قرمز (٪)<input type="number" id="q45" step="5" min="0" max="100"><span class="lb" id="a45">—</span></label>
</div>
<div class="sec">بوق</div>
<div class="bqr">
<label>دوره بوق (ms، صفر=خاموش)<input type="number" id="q46" step="500" min="0" max="600000"><span class="lb" id="a46">—</span></label>
<label>مدت هر بوق (ms)<input type="number" id="q47" step="50" min="0" max="600000"><span class="lb" id="a47">—</span></label>
<label>تعداد بوق<input type="number" id="q48" step="1" min="0" max="10"><span class="lb" id="a48">—</span></label>
<label>گپ بین بوق‌ها (ms)<input type="number" id="q49" step="50" min="0" max="5000"><span class="lb" id="a49">—</span></label>
</div>
<div class="lb">روند: قفل‌شدن پرچم قطع‌باتری ← قرمز چشمک + بوق دوره‌ای (سبز ثابت) ← پاک‌شدن پرچم ← بازگشت به سناریوی قبلی. پیش‌فرض: سه بوق کوتاه. آستانه‌های تشخیص قطع/برگشت در کارت «نظارت باتری» (زیرتب نظارت و ایمنی، ۲۷..۳۲) است.</div>
</div>
<div class="cd" id="ucard3" style="display:none">
<div class="hd"><b>سناریو ۳ — دشارژ (بی‌ورودی)</b><span class="lb">· شناسه‌های ۵۰..۶۷ · باندها + چشمک سبز</span></div>
<div class="sec">چشمک سبز <span class="lb">(ms)</span></div>
<div class="bqr">
<label>دوره چشمک سبز (ms)<input type="number" id="q66" step="50" min="100" max="10000"><span class="lb" id="a66">—</span></label>
<label>حداقل خاموشی سبز (ms)<input type="number" id="q67" step="5" min="0" max="10000"><span class="lb" id="a67">—</span></label>
</div>
<div class="sec">باندهای درصد <span class="lb">(٪ باتری؛ همیشه شروع ≥ دو-بوق ≥ سه-بوق ≥ بحرانی)</span></div>
<div class="bqr">
<label>شروع بوق (٪)<input type="number" id="q50" step="1" min="0" max="100"><span class="lb" id="a50">—</span></label>
<label>باند دو-بوق (٪)<input type="number" id="q51" step="1" min="0" max="100"><span class="lb" id="a51">—</span></label>
<label>باند سه-بوق (٪)<input type="number" id="q52" step="1" min="0" max="100"><span class="lb" id="a52">—</span></label>
<label>باند بحرانی (٪)<input type="number" id="q53" step="1" min="0" max="100"><span class="lb" id="a53">—</span></label>
</div>
<div class="sec">فاصله و دوره <span class="lb">(ms؛ صفر=خاموش)</span></div>
<div class="bqr">
<label>فاصله بوق ۱/۲تایی (ms)<input type="number" id="q54" step="1000" min="0" max="600000"><span class="lb" id="a54">—</span></label>
<label>فاصله بوق ۳تایی (ms)<input type="number" id="q55" step="1000" min="0" max="600000"><span class="lb" id="a55">—</span></label>
<label>دوره بوق بحرانی (ms)<input type="number" id="q56" step="500" min="0" max="600000"><span class="lb" id="a56">—</span></label>
<label>دیوتی بوق بحرانی (٪)<input type="number" id="q57" step="5" min="0" max="100"><span class="lb" id="a57">—</span></label>
<label>تعداد بوق بحرانی<input type="number" id="q58" step="1" min="0" max="10"><span class="lb" id="a58">—</span></label>
</div>
<div class="sec">مدت، تعداد و گپ <span class="lb">(ms / عدد)</span></div>
<div class="bqr">
<label>مدت هر بوق ۱/۲تایی (ms)<input type="number" id="q59" step="50" min="0" max="600000"><span class="lb" id="a59">—</span></label>
<label>مدت هر بوق ۳تایی (ms)<input type="number" id="q60" step="50" min="0" max="600000"><span class="lb" id="a60">—</span></label>
<label>طول بوق بحرانی یک‌باره (ms)<input type="number" id="q61" step="500" min="0" max="120000"><span class="lb" id="a61">—</span></label>
<label>تعداد بوق باند ۱<input type="number" id="q62" step="1" min="0" max="10"><span class="lb" id="a62">—</span></label>
<label>تعداد بوق باند ۲<input type="number" id="q63" step="1" min="0" max="10"><span class="lb" id="a63">—</span></label>
<label>تعداد بوق باند ۳<input type="number" id="q64" step="1" min="0" max="10"><span class="lb" id="a64">—</span></label>
<label>گپ بوق دشارژ (ms)<input type="number" id="q65" step="10" min="0" max="5000"><span class="lb" id="a65">—</span></label>
</div>
<div class="lb">روند: بالای «شروع بوق» بی‌صدا (سبز چشمک با درصد) ← هر باند بوق خودش با فاصله/مدت/تعداد خودش ← زیر «باند بحرانی»: LEDها خاموش و الگوی بحرانی فقط یک‌بار به‌اندازهٔ «طول یک‌باره» پخش و بعد سکوت تا برگشت باتری. با پیش‌فرض‌ها: بالای ۴۰٪ بی‌صدا؛ ۲۰..۴۰ یک بوق، ۱۰..۲۰ دو بوق (هر ۶۰ ثانیه)؛ ۱..۱۰ سه بوق (هر ۲۰ ثانیه)؛ زیر ۱٪ یک بوق ۱۰ثانیه‌ای. گپ (۶۵) مشترک همهٔ باندهاست.</div>
</div>
<div class="cd" id="ucard4" style="display:none">
<div class="hd"><b>سناریو ۴ — شارژ عادی</b><span class="lb">· شناسه‌های ۶۸/۶۹ · چشمک زرد</span></div>
<div class="sec">چشمک زرد <span class="lb">(ms)</span></div>
<div class="bqr">
<label>دوره چشمک زرد (ms)<input type="number" id="q68" step="50" min="100" max="10000"><span class="lb" id="a68">—</span></label>
<label>حداقل خاموشی زرد (ms)<input type="number" id="q69" step="5" min="0" max="10000"><span class="lb" id="a69">—</span></label>
</div>
<div class="lb">روند: حین شارژ واقعی، مدت روشن‌بودن زرد = مانده تا فول (باتری پرتر ← چشمک کوتاه‌تر) ← فول (۱۰۰٪، خروج زیر ۹۵٪) یا شارژر بیکار ← سبز ثابت.</div>
</div>
<div class="cd" id="ucard5" style="display:none">
<div class="hd"><b>آستانه‌های باتری و نگاشت درصد</b><span class="lb">· شناسه‌های ۷۲..۷۵</span></div>
<div class="sec">آلارم باتری کم <span class="lb">(mV)</span></div>
<div class="bqr">
<label>آستانه آلارم باتری کم (mV)<input type="number" id="q72" step="100" min="15000" max="24000"><span class="lb" id="a72">—</span></label>
<label>پاک‌شدن آلارم باتری کم (mV)<input type="number" id="q73" step="100" min="15000" max="24000"><span class="lb" id="a73">—</span></label>
</div>
<div class="sec">نگاشت ولتاژ به درصد <span class="lb">(mV)</span></div>
<div class="bqr">
<label>کف نگاشت درصد (mV)<input type="number" id="q74" step="100" min="15000" max="25000"><span class="lb" id="a74">—</span></label>
<label>سقف نگاشت درصد (mV)<input type="number" id="q75" step="100" min="25000" max="32000"><span class="lb" id="a75">—</span></label>
</div>
<div class="lb">روند: افت باتری زیر آستانه ← پرچم باتری کم (پیوسته) + ⚠ در آینه ← صعود تا سطح پاک‌شدن ← پاک‌شدن پرچم. نگاشت ۷۴/۷۵ درصد همهٔ سناریوها را می‌سازد؛ سقف همیشه دست‌کم ۱۰۰mV بالای کف است.</div>
</div>
<input type="hidden" id="q76" value="">
<div class="bqr"><button class="sb sb2" onclick="sdef()">بازگردانی پیش‌فرض کارخانهٔ سناریوها</button></div>
</div>
<div class="sgx" id="s2">
<div class="cd">
<div class="hd"><b>نظارت باتری</b><span class="lb">· شناسه‌های ۲۷..۳۲ · روی فلش برد ذخیره می‌شود (~۱٫۵ ثانیه پس از آخرین تغییر)</span></div>
<div id="aw" style="margin:2px 0 0"></div>
<div class="sec">قطع باتری <span class="lb">(mV / ms)</span></div>
<div class="bqr">
<label>آستانهٔ قطع باتری (mV)<input type="number" id="q27" step="50" min="14000" max="15000"><span class="lb" id="a27">—</span></label>
<label>دبانس قطع (ms)<input type="number" id="q28" step="10" min="50" max="1000"><span class="lb" id="a28">—</span></label>
</div>
<div class="sec">غیبت / بازگشت باتری <span class="lb">(mV / ms)</span></div>
<div class="bqr">
<label>آستانهٔ غیبت (mV)<input type="number" id="q29" step="100" min="3000" max="8000"><span class="lb" id="a29">—</span></label>
<label>آستانهٔ بازگشت (mV)<input type="number" id="q30" step="100" min="4000" max="9000"><span class="lb" id="a30">—</span></label>
<label>دبانس غیبت (ms)<input type="number" id="q31" step="50" min="100" max="5000"><span class="lb" id="a31">—</span></label>
<label>دبانس بازیابی (ms)<input type="number" id="q32" step="50" min="100" max="5000"><span class="lb" id="a32">—</span></label>
</div>
<div class="lb">قطع باتری: اگر هر نیمه حین پمپ بالای این ولتاژ برود، سیم باتری قطع فرض می‌شود (پیش‌فرض ۱۴۸۰۰)؛ باید بالای سقف تجاوز+۵۰ و زیر قطع OV−۱۰۰ بماند وگرنه برد گیره‌اش می‌زند.
دبانس قطع: شرط بالا باید این‌قدر میلی‌ثانیه پیوسته برقرار بماند تا لچ شود (۱۵۰).
غیبت/برگشت: زیر آستانهٔ غیبت (۶۰۰۰) باتری نیست؛ بالای بازگشت (۷۰۰۰) برگشته — همیشه ۵۰۰mV از هم فاصله دارند.
دبانس غیبت/بازیابی: پایداری لازم برای اعلام غیبت و اعلام سلامتی (۱۰۰۰/۱۰۰۰).</div>
</div>
<div class="cd">
<div class="hd"><b>پنجرهٔ ورودی سالم</b><span class="lb">· شناسه‌های ۳۳..۳۴ · روی فلش برد ذخیره می‌شود</span></div>
<div class="bqr">
<label>کف ورودی سالم (mV)<input type="number" id="q33" step="100" min="18000" max="24000"><span class="lb" id="a33">—</span></label>
<label>سقف ورودی سالم (mV)<input type="number" id="q34" step="100" min="24000" max="30000"><span class="lb" id="a34">—</span></label>
</div>
<div class="lb">تشخیص «ورودی حاضر» فقط داخل این پنجره است (پیش‌فرض ۲۱۰۰۰..۲۸۰۰۰)؛ کف و سقف همیشه ۱۰۰۰mV از هم فاصله دارند. بیرون پنجره، شارژر منتظر ورودی می‌ماند.</div>
</div>
<div class="cd">
<div class="hd"><b>سقف‌های ایمنی شارژر</b><span class="lb">· شناسه‌های ۳۵..۳۷ · فقط پایین‌بردنی — هرگز بالای سقف کارخانه نمی‌روند · روی فلش برد ذخیره می‌شود</span></div>
<div class="bqr">
<label>خطای سخت جریان (mA)<input type="number" id="q35" step="10" min="150" max="950"><span class="lb" id="a35">—</span></label>
<label>قطع اضافه‌ولتاژ OV (mV)<input type="number" id="q36" step="50" min="14000" max="15000"><span class="lb" id="a36">—</span></label>
<label>کف اعتبار باتری (mV)<input type="number" id="q37" step="100" min="0" max="8000"><span class="lb" id="a37">—</span></label>
</div>
<div class="lb">خطای سخت جریان: بالای این مقدار کانال ریست و متوقف می‌شود (پیش‌فرض ۹۵۰)؛ همیشه بالای جریان بالک+۵۰ نگه داشته می‌شود تا تنظیم سالم تریپ نکند.
قطع OV: بالای این ولتاژ باتری نامعتبر و سوئیچینگ متوقف می‌شود (پیش‌فرض ۱۵۰۰۰)؛ همیشه بالای سقف تجاوز+۱۵۰ است.
کف اعتبار: زیر این ولتاژ باتری نامعتبر شمرده می‌شود (پیش‌فرض ۲۰۰۰).
پس از هر تغییر، مقدار «اعمال‌شدهٔ» برد کنار همان فیلد نشان داده می‌شود — اگر با درخواست شما فرق دارد یعنی گیره خورده تا مجموعه سازنده بماند.
نگهبان ترکیب مثل تب تنظیمات: عدد ناسازگار هشدار قرمز و تأیید قبل از ارسال می‌گیرد.</div>
<div class="bqr"><button class="sb sb2" onclick="adef()">بازگردانی پیش‌فرض کارخانهٔ نظارت و ایمنی</button></div>
</div>
</div>
<div class="sgx" id="s3">
<div class="cd">
<div class="hd"><b>پشتیبان‌گیری همهٔ تنظیمات</b><span class="lb">· یک بکاپ برای کل بخش تنظیمات — خروجی/ورودی JSON همهٔ ۷۱ مقدار ماندگار (۰..۱۴، ۲۰..۷۵)</span></div>
<div class="bqr">
<button class="sb sb2" onclick="xexp()">⬇ خروجی (دانلود JSON)</button>
<label class="sb" style="cursor:pointer">⬆ ورودی (انتخاب فایل)<input type="file" id="xim" accept=".json,application/json" style="display:none"></label>
<span class="lb" id="xst">—</span>
</div>
<div class="lb">خروجی، همهٔ مقادیر «اعمال‌شدهٔ» برد (کالیبراسیون، فیلتر، فعال‌سازی/سقف‌ها، پروفایل، آلارم‌ها، سناریوها) را در یک فایل JSON ذخیره می‌کند. ورودی همان فایل را می‌خواند و مقدارها را یکی‌یکی روی برد اعمال می‌کند (با تأیید شما؛ برد هر مقدار را گیره می‌زند و نتیجه کنار همان فیلد دیده می‌شود؛ شناسه‌های بدون فیلد بی‌صدا اعمال می‌شوند). گذراها (۱۵..۱۹ و میوت ۷۶) جزو پشتیبان نیستند.</div>
</div>
</div>
</div>
</main>

<script>
const $=i=>document.getElementById(i);
const ST=['خاموش','Bulk','Absorb','Float','راه‌اندازی','انتظار JIT','انتظار ورودی','خطای نهایی','باتری قطع','دستی'];
const SC=['','g','g','g','y','r','y','r','r','y'];
/* ثابت‌های بخش 5.3 سند */
const K_UV=3300/4095*11/10*1000/101,K_MA=K_UV/10,K24=3300/4095*76000/6800,K24B=3300/4095*69200/6800,K12=3300/4095*41000/6800;
/* شناسه: [عنوان, واحد, کمینه, بیشینه, نوع(n عدد، b کلید), توضیح] */
const P={
7:['پنجرهٔ مدین','نمونه',1,15,'n','مرحلهٔ اول فیلتر، هر عدد ۱ تا ۱۵ (زوج هم مجاز)؛ ۱ و ۲ = خاموش، ۳ = پیش‌فرض، بزرگ‌تر = حذف پالس قوی‌تر با تاخیر بیشتر'],
8:['پنجرهٔ میانگین','نمونه',1,300,'n','مرحلهٔ دوم فیلتر، هر عدد ۱ تا ۳۰۰: میانگین آخرین W خروجی مدین (هر نمونه ۱ms = ۱ms تاریخچه). ۱ = خاموش، ۱۰ = پیش‌فرض. برای دیدن صاف‌کردن روی پنل (TLM هر ۱۰۰ms می‌آید) W را ۲۰۰..۳۰۰ بگذارید؛ در مود خودکار شارژر بالای ~۵۰ توصیه نمی‌شود (کندی حلقهٔ تنظیم ۱۰۰Hz)'],
13:['سقف دیوتی ۱','‰',0,500,'n','سقف duty کانال ۱ به هزارم درصد (۵۰۰ = ۵۰٪)؛ هر دیوتی اعمالی (تنظیم خودکار، فیکس، دستی) به این سقف گیره می‌شود'],
14:['سقف دیوتی ۲','‰',0,500,'n','سقف duty کانال ۲ به هزارم درصد (۵۰۰ = ۵۰٪)؛ مثل کانال ۱']};
/* ولتاژها: [عنوان, اندیس t, شناسهٔ آفست, ضریب مقسم] */
const V=[['ورودی',14,4,K24],['پک ۲۴V',15,5,K24B],['نود ۱۲V',16,6,K12],['باتری بالا',18],['باتری پایین',17]];
var D=null;/* var (نه let) تا در تست هاست هم قابل‌نوشتن باشد */
const v2=mv=>(mv/1000).toFixed(2),pc=pm=>(pm/10).toFixed(1)+'%';
function send(id,v){const a=$('a'+id);if(a)a.textContent='…';fetch('/s?id='+id+'&v='+v,{method:'POST'}).then(r=>{if(!r.ok)throw 0;}).catch(()=>{if(a)a.textContent='خطا';});}
function num(id){const e=$('i'+id),p=P[id],v=Math.round(+e.value);if(e.value===''||isNaN(v))return;send(id,Math.min(p[3],Math.max(p[2],v)));e.value='';e.blur();}
function ctl(id){const p=P[id];
 return `<input type="number" id="i${id}" min="${p[2]}" max="${p[3]}" placeholder="${p[2]<0?'±'+p[3]:p[2]+'…'+p[3]}" onkeydown="if(event.key=='Enter')num(${id})"><button class="sb" onclick="num(${id})">ثبت</button>`;}
const row=(id,x)=>`<div class="rw"><div>${P[id][0]} <span class="lb">${P[id][1]}</span><span class="ap n" id="a${id}">—</span></div><div class="ct">${x||''}${ctl(id)}</div><div class="h" onclick="this.classList.toggle('o')">${P[id][5]}</div></div>`;

/* ---------- ساخت صفحه: ولتاژها + فیلتر (مشترک) ---------- */
$('vs').innerHTML=V.map((v,i)=>`<div class="vt"><small>${v[0]}</small><b class="n" id="v${i}">—</b><div class="fx" id="fv${i}"></div>${i<3?`
<div class="ct vc"><input type="number" step="any" id="vm${i}" placeholder="مولتی‌متر V" onkeydown="if(event.key=='Enter')vcal(${i})"><button class="sb sb2" onclick="vcal(${i})">اعمال</button></div>
<div class="lb">آفست <span class="ap n" id="a${v[2]}">—</span> mV</div>`:''}</div>`).join('');
/* v1.14b (user order 2026-09-26): پنجرهٔ مدین/میانگین به تب «تنظیمات» رفت؛ اینجا فقط وضعیت زندهٔ فیلتر و نمونه‌های نمودار می‌مانند */
$('fg').innerHTML='<div class="lb" id="fspan" style="margin-top:6px">—</div><div class="bctl" style="margin-top:8px"><label class="lb">نمونه‌های نمودار <input type="number" id="hN" data-s min="10" max="600" value="100" style="width:64px"></label></div>';
/* ---------- دو ستون جدا: شارژر ۱ و شارژر ۲ ---------- */
$('ch').innerHTML=[1,2].map(n=>`<div class="cd"><div class="hd"><b>شارژر ${n} <span class="lb">· باتری ${n==1?'بالا':'پایین'}</span></b><span class="tg" id="st${n}">—</span></div>
<div class="big"><span class="lb">جریان تخمینی باتری (iest)</span><b class="n" id="ie${n}">—</b></div>
<div class="big"><span class="lb">duty <span id="dc${n}"></span></span><span class="n" id="du${n}">—</span></div><div class="bar"><i id="db${n}"></i><u id="cl${n}"></u></div>
<div class="sec">زنجیرهٔ اندازه‌گیری و محاسبه</div>
<table>${[['ADC خام','count',0],['ولتاژ شنت','µV',1],['جریان بدون فیلتر','mA',2],['جریان فیلترشده','mA',3],['تخمین باتری (iest)','mA',4]].map(r=>`<tr><td>${r[0]}<div class="fx" id="f${n}${r[2]}"></div></td><td class="n"><b id="c${n}${r[2]}">—</b> <span class="lb">${r[1]}</span></td></tr>`).join('')}</table>
<div class="lb kc">ثابت‌ها: ADC دوازده‌بیتی، ۳۳۰۰mV، R41/R42 = 1k/10k، LM358 × 101، شنت 10 mOhm</div>
<canvas id="cv${n}"></canvas><div class="lg"><span><i style="background:#5b6784"></i>بدون فیلتر · نوسان <b class="n" id="pu${n}">—</b> mA</span><span><i style="background:#4f8cff"></i>فیلترشده · نوسان <b class="n" id="pf${n}">—</b> mA</span><span class="hnl"></span></div>
<button class="bt" id="tg${n}">—</button></div>`).join('');
[1,2].forEach(n=>$('tg'+n).onclick=()=>{const c=D&&D.p[10+n];if(c!==0&&!confirm('PWM شارژر '+n+' فوراً قطع شود؟'))return;send(10+n,c===0?1:0);});
/* کارت کنترل دستی دیوتی — v1.10 در تب «پنل» (دستور کاربر ۲۰۲۶-۰۹-۲۵) + سقف دیوتی هر کانال */
$('mc').innerHTML=`<div class="cd"><div class="ti">کنترل دستی دیوتی (تست جریان)</div><div class="ds">مود دستی شارژر خودکار و محافظت باتری‌ها را متوقف می‌کند و دیوتی را خودتان تعیین می‌کنید؛ فقط حضور ۲۴V، قطع JIT، قطع ۱۵٫۰V و سقف دیوتی می‌ماند. پنل را نبندید — ۱۰ ثانیه بعد از بستن، مود دستی خاموش و دیوتی صفر می‌شود. بعد از تریپ JIT همان دیوتی را دوباره اعمال کنید.</div>
<div class="bctl"><span class="lb">مود دستی</span><button class="sw w" id="s19">—</button>
<label class="lb">دیوتی ۱ ٪ <input type="number" step="any" id="qm1" data-s style="width:64px"></label><button class="sb" onclick="qset(1)">اعمال</button>
<label class="lb">دیوتی ۲ ٪ <input type="number" step="any" id="qm2" data-s style="width:64px"></label><button class="sb" onclick="qset(2)">اعمال</button>
<button class="sb off2" id="ao">هر دو = 0</button></div>
<div class="frr" id="clr"></div>
<div class="lb" id="mq" style="margin-top:6px"></div></div>`;
$('clr').innerHTML=row(13)+row(14);
/* ---------- تاریخچهٔ نمودار هر کانال ---------- */
let LS=-1;const hn=()=>{const v=Math.round(+$('hN').value);return !v?100:Math.min(600,Math.max(10,v));},H=[0,1].map(()=>({u:[],f:[]}));
function vcal(k){const R=V[k],m=Math.round(+$('vm'+k).value*1000),shown=D&&D.t[R[1]],off=D&&D.p[R[2]];if(!(m>0))return alert('عدد مولتی‌متر را به ولت وارد کنید (مثلاً 13.05).');if(off==null)return;
 const no=Math.min(5000,Math.max(-5000,off+m-shown));if(confirm(R[0]+': آفست '+off+' ← '+no+' mV\n(نمایش '+v2(shown)+' V، مولتی‌متر '+v2(m)+' V)')){send(R[2],no);$('vm'+k).value='';}}
/* نمودار زندهٔ فیلتر هر کانال */
function chart(){document.querySelectorAll('.hnl').forEach(e=>e.textContent=hn()+' نمونهٔ اخیر');[0,1].forEach(ci=>{const c=$('cv'+(ci+1)),w=c.clientWidth,h=c.clientHeight,dp=devicePixelRatio||1;if(!w)return;
 if(c.width!=Math.round(w*dp)){c.width=Math.round(w*dp);c.height=Math.round(h*dp);}
 const x=c.getContext('2d');x.setTransform(dp,0,0,dp,0,0);x.clearRect(0,0,w,h);const s=H[ci];if(s.u.length<2)return;
 let lo=Math.min(...s.u,...s.f),hi=Math.max(...s.u,...s.f);if(hi-lo<10){const m=(hi+lo)/2;lo=m-5;hi=m+5;}const pd=(hi-lo)*.12,a=lo-pd,z=hi+pd;
 const X=i=>w-8-(s.u.length-1-i)*(w-16)/(hn()-1),Y=v=>h-8-(v-a)/(z-a)*(h-16);
 const ln=(A,col,lw)=>{x.beginPath();A.forEach((v,i)=>i?x.lineTo(X(i),Y(v)):x.moveTo(X(i),Y(v)));x.strokeStyle=col;x.lineWidth=lw;x.stroke();};
 x.fillStyle='#6f7a93';x.font='11px Vazirmatn,sans-serif';x.fillText(Math.round(hi)+' mA',8,16);x.fillText(Math.round(lo)+' mA',8,h-10);
 ln(s.u,'#5b6784',1);ln(s.f,'#4f8cff',2);const pp=A=>{const B=A.slice(-50);return Math.max(...B)-Math.min(...B);};$('pu'+(ci+1)).textContent=pp(s.u);$('pf'+(ci+1)).textContent=pp(s.f);});}
/* تعویض تب: پنل و داده‌برداری بنچ */
let TAB=0;document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{TAB=+b.dataset.t;document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('a',x===b));document.querySelectorAll('.pgx').forEach((x,i)=>x.classList.toggle('a',i==TAB));if(D)draw(D);});
/* v1.15b: زیرتب داخل تنظیمات — ۰=شارژ و فیلتر، ۱=آلارم‌ها */
let STAB=0;document.querySelectorAll('#sbt button').forEach(b=>b.onclick=()=>{STAB=+b.dataset.s;document.querySelectorAll('#sbt button').forEach(x=>x.classList.toggle('a',x===b));document.querySelectorAll('.sgx').forEach((x,i)=>x.classList.toggle('a',i==STAB));if(D)draw(D);});
let UCARD=1;function usel(n){UCARD=n;for(let k=1;k<=5;k++){const c=$('ucard'+k);if(c)c.style.display=k===n?'':'none';}document.querySelectorAll('#usel button').forEach(b=>b.classList.toggle('a',+b.dataset.u===n));}
document.querySelectorAll('#usel button').forEach(b=>b.onclick=()=>usel(+b.dataset.u));
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
function hist(d){const t=d.t;if(d.on==1&&d.seq!==LS){LS=d.seq;[0,1].forEach(c=>{const b=c*7,s=H[c];s.u.push(t[b+2]);s.f.push(t[b+3]);if(s.u.length>hn()){s.u.shift();s.f.shift();}});}}
function qfill(){if(!D||!D.p)return;for(const id of [7,8,20,21,22,23,24,25,26]){const e=$('q'+id),a=$('a'+id);if(!e)continue;if(document.activeElement!==e&&e.value==='')e.value=D.p[id]==null?'':D.p[id];if(a&&!(D.q&(1<<id)))a.textContent=D.p[id]==null?'—':D.p[id];}}
function qdef(){[[20,14400],[21,14300],[22,14600],[23,13500],[24,12800],[25,650],[26,50]].forEach(x=>{$('q'+x[0]).value=x[1];send(x[0],x[1]);});qgraph();}
/* ===== v1.14: نمودار مراحل شارژ — مقدار هر خط از فیلد تایپ‌نشده/متفاوت با مقدار اعمال‌شده می‌آید (پیش‌نمایش خط‌چین) ===== */
const QDEF=[14400,14300,14600,13500,12800,650,50];
function qv(id){const e=$('q'+id),d=D&&D.p&&D.p[id]!=null?D.p[id]:QDEF[id-20];
 if(e&&e.value!==''){const v=parseInt(e.value,10);if(!isNaN(v))return{v,d,p:v!==d?1:0};}
 return{v:d,d,p:0};}
/* v1.14d: نگهبان ترکیب پروفایل — آینهٔ قوانین Charger_ClampProfile روی برد.
   هر قانون: [فیلد اصلی، فیلد مرجع] + پیام فارسی. خروجی خالی = ترکیب سالم. */
function qchk(){const w=[],a=qv(20).v,e=qv(21).v,o=qv(22).v,f=qv(23).v,r=qv(24).v,im=qv(25).v,tp=qv(26).v;
 const bad=(v,lo,hi)=>!(v>=lo&&v<=hi);
 if(bad(a,11000,14600))w.push({ids:[20],msg:'ابزورب باید ۱۱۰۰۰..۱۴۶۰۰ باشد'});
 else{
  if(bad(e,a-500,a-50))w.push({ids:[21,20],msg:'ورود ابزورب باید ابزورب−۵۰۰ تا ابزورب−۵۰ باشد ('+(a-500)+'..'+(a-50)+')'});
  if(bad(o,a+100,Math.min(a+400,14750)))w.push({ids:[22,20],msg:'سقف تجاوز باید ابزورب+۱۰۰ تا ابزورب+۴۰۰ (سقف ۱۴۷۵۰) باشد'});
  if(bad(f,9000,a-300))w.push({ids:[23,20],msg:'شناور باید ۹۰۰۰..ابزورب−۳۰۰ باشد (≤ '+(a-300)+')'});
  else if(bad(r,8000,f-300))w.push({ids:[24,23],msg:'بازگشت باید ۸۰۰۰..شناور−۳۰۰ باشد (≤ '+(f-300)+')'});
 }
 if(bad(im,100,900))w.push({ids:[25],msg:'جریان بالک باید ۱۰۰..۹۰۰ باشد'});
 if(bad(tp,10,Math.min(300,im)))w.push({ids:[26,25],msg:'تیپر باید ۱۰..سقف بالک باشد (≤ '+Math.min(300,im)+')'});
 return w;}
function qgraph(){const g=$('qg');if(!g)return;
 const q={a:qv(20),e:qv(21),o:qv(22),f:qv(23),r:qv(24)},im=qv(25),tp=qv(26);
 /* v1.14e (دستور کاربر ۲۰۲۶-۰۹-۲۶ «حداقل ۵۰٪ بلندتر»): H=840 تا مرز
    ناحیه‌ها در هم نرود؛ ناحیه‌ها از مقادیر اعمال‌شده (.d) — چون برد گیره
    می‌زند هرگز وارونه/هم‌پوشان نمی‌شوند؛ تایپِ هنوز-اعمال‌نشده فقط خط‌چین */
 const lo=Math.max(7600,Math.min(q.r.d,12000)-500),hi=15060,W=760,H=840,X0=54,X1=738;
 const Y=mv=>Math.round(H-36-(H-70)*(mv-lo)/(hi-lo));
 const V=mv=>(mv/1000).toFixed(2);
 /* برچسب‌ها جدا جمع و با کمینهٔ فاصله رندر می‌شوند تا در ناحیه‌های باریک در هم نروند */
 const ZL=[],LL=[];
 const zone=(mv1,mv2,fill,txt,c)=>{const y1=Y(Math.max(mv1,mv2)),y2=Y(Math.min(mv1,mv2));
  if(txt)ZL.push({y:y1+15,txt,c});
  return`<rect x="${X0}" y="${y1}" width="${X1-X0}" height="${Math.max(3,y2-y1)}" fill="${fill}"/>`;};
 const aln=(mv,c)=>`<line x1="${X0}" y1="${Y(mv)}" x2="${X1}" y2="${Y(mv)}" stroke="${c}" stroke-width="2.5"/>`;
 const pvln=(o,c)=>o.p?`<line x1="${X0}" y1="${Y(o.v)}" x2="${X1}" y2="${Y(o.v)}" stroke="${c}" stroke-width="2.5" stroke-dasharray="8 5"/>`:'';
 const lbl=(o,c,txt)=>{LL.push({y:Y(o.p?o.v:o.d)-5,txt,c,pv:o.p});};
 const put=(A,x,anchor,fs)=>{A.sort((p,q2)=>p.y-q2.y);let last=4;
  return A.map(o=>{const yc=Math.min(Math.max(o.y,20),H-14),y=Math.max(last+16,yc),sh=y-yc>3;last=y;
   return (sh?`<line x1="${x}" y1="${yc+4}" x2="${x}" y2="${y-4}" stroke="${o.c}" stroke-width="1" opacity=".6"/>`:'')+
   `<text x="${x}" y="${y}" text-anchor="${anchor}" font-size="${fs}" font-weight="700" fill="${o.c}">${o.txt}${o.pv?' · پیش‌نمایش':''}</text>`;}).join('');};
 let s=`<svg viewBox="0 0 ${W} ${H}" style="width:100%;min-width:640px;font-family:inherit">`;
 /* v1.14e: حاشور کم‌رنگ ناحیهٔ بالک (دستور کاربر ۲۰۲۶-۰۹-۲۶) */
 s+=`<defs><pattern id="bkh" width="9" height="9" patternTransform="rotate(45)" patternUnits="userSpaceOnUse"><rect width="9" height="9" fill="rgba(79,140,255,.06)"/><line x1="0" y1="0" x2="0" y2="9" stroke="rgba(110,168,255,.30)" stroke-width="1.2"/></pattern></defs>`;
 s+=`<rect x="${X0}" y="18" width="${X1-X0}" height="${H-52}" fill="#0d1320" stroke="#232c40" rx="6"/>`;
 for(let mv=Math.ceil(lo/500)*500;mv<=hi;mv+=500){const y=Y(mv);
  s+=`<line x1="${X0}" y1="${y}" x2="${X1}" y2="${y}" stroke="#1c2436" stroke-width="1"/>`+
     `<text x="${X0-4}" y="${y+4}" text-anchor="end" font-size="10" fill="#8089a0">${(mv/1000).toFixed(1)}</text>`;}
 s+=zone(hi,15000,'rgba(255,92,92,.16)','','#ff5c5c');
 s+=zone(15000,q.o.d,'rgba(255,92,92,.09)','ناحیهٔ تجاوز (Over) — کاهش سریع duty','#ff7373');
 s+=zone(q.o.d,q.e.d,'rgba(245,185,66,.08)','ناحیهٔ ابزورب (Absorb)','#f5b942');
 s+=zone(q.e.d,q.f.d,'url(#bkh)','ناحیهٔ بالک (Bulk) — شارژ با جریان ثابت','#6ea8ff');
 s+=zone(q.f.d,q.r.d,'rgba(46,204,143,.09)','ناحیهٔ شناور (Float)','#2ecc8f');
 s+=zone(q.r.d,lo,'rgba(79,140,255,.10)','زیر بازگشت (Reentry) — شارژ دوباره از بالک','#6ea8ff');
 s+=`<line x1="${X0}" y1="${Y(15000)}" x2="${X1}" y2="${Y(15000)}" stroke="#ff5c5c" stroke-width="1.5" stroke-dasharray="3 4"/>`;
 LL.push({y:Y(15000)-5,txt:'قطع سخت (Cutoff) ۱۵V',c:'#ff5c5c'});
 s+=aln(q.o.d,'#f08c2e')+pvln(q.o,'#f08c2e');lbl(q.o,'#f08c2e','سقف تجاوز (Over)');
 s+=aln(q.a.d,'#f5b942')+pvln(q.a,'#f5b942');lbl(q.a,'#f5b942','ابزورب (Absorb)');
 s+=aln(q.e.d,'#d98e2b')+pvln(q.e,'#d98e2b');lbl(q.e,'#d98e2b','ورود ابزورب (Absorb Enter)');
 s+=aln(q.f.d,'#2ecc8f')+pvln(q.f,'#2ecc8f');lbl(q.f,'#2ecc8f','شناور (Float)');
 s+=aln(q.r.d,'#4f8cff')+pvln(q.r,'#4f8cff');lbl(q.r,'#4f8cff','بازگشت به بالک (Reentry)');
 /* موقعیت زندهٔ هر باتری (دستور کاربر ۲۰۲۶-۰۹-۲۶): نقطهٔ رنگی روی ولتاژ خودش
    در ستون مخصوصش + برچسب وضعیت زیر نمودار؛ ناحیه‌ها خودشان داستان مراحل را می‌گویند */
 let lg='';
 if(D&&D.t){const tt=D.t;
  const BST={0:['خاموش (Off)','#9aa5bd'],1:['بالک (Bulk)','#4f8cff'],2:['ابزورب (Absorb)','#f5b942'],3:['شناور (Float)','#2ecc8f'],4:['راه‌اندازی (Bring-up)','#f5b942'],5:['انتظار JIT (JIT wait)','#ff5c5c'],6:['انتظار ورودی (No input)','#f5b942'],7:['خطای نهایی (Final fault)','#ff5c5c'],8:['باتری قطع (Battery lost)','#ff5c5c'],9:['دستی (Manual)','#f5b942']};
  const bats=[['باتری پایین (Vlow)',tt[17],tt[13],tt[10],'#c084fc',0.60],['باتری بالا (Vhigh)',tt[18],tt[6],tt[3],'#fbbf24',0.82]];
  bats.forEach(b=>{
   if(b[1]>lo&&b[1]<hi){const y=Y(b[1]),x=X0+Math.round((X1-X0)*b[5]);
    s+=`<line x1="${X0}" y1="${y}" x2="${X1}" y2="${y}" stroke="${b[4]}" stroke-width="1.6" stroke-dasharray="2 3"/>`;
    s+=`<circle cx="${x}" cy="${y}" r="7" fill="${b[4]}" stroke="#0d1320" stroke-width="2.5"/>`;
    s+=`<text x="${x}" y="${Math.min(y+24,H-10)}" text-anchor="middle" font-size="10.5" font-weight="700" fill="${b[4]}">${b[0].split(' (')[0]} ${V(b[1])}V</text>`;}});
  lg=bats.map(b=>{const st=BST[b[2]]||('#'+b[2]);
   return `<span class="tg" style="background:${st[1]}22;color:${st[1]};border:1px solid ${st[1]}66">● ${b[0]}: <b>${V(b[1])}V</b> · ${b[3]}mA · ${st[0]}</span>`;}).join(' ')+
   `<span class="lb"> · بالک ≤ ${im.v}mA · تیپر < ${tp.v}mA · پس از هر تغییر ~۱٫۵ ثانیه بعد روی فلش برد ذخیره می‌شود</span>`;
 }else lg='در انتظار دادهٔ برد…';
 s+=put(ZL,X0+8,'start','11.5')+put(LL,X1-4,'end','12');
 s+=`<text x="${X0}" y="12" font-size="10" fill="#8089a0">ولتاژ باتری / Battery voltage (V)</text></svg>`;
 g.innerHTML=s;const e=$('qgl');if(e)e.innerHTML=lg;
 /* نگهبان: هشدار بالای نمودار + قرمزکردن فیلد مقصر */
 const w=qchk(),we=$('qw');
 if(we){we.innerHTML=w.length?('⚠ ترکیب نامعتبر — برد این‌ها را گیره می‌زند: '+w.map(x=>x.msg).join('؛ ')):'';
  we.style.cssText=w.length?'margin:2px 0 6px;color:#ff7373;font-size:12.5px;line-height:1.9':'margin:2px 0 0';}
 for(const id of [20,21,22,23,24,25,26]){const ne=$('q'+id);if(ne)ne.style.borderColor=w.some(x=>x.ids.includes(id))?'#b8323f':'';}}
/* [EN] bind the profile inputs: on change, POST /s (fire-and-forget; the ack span next to the field shows the APPLIED value reported by the STM32). v1.14d: a typed value that breaks the profile rules asks for confirmation first, because the board will clamp it. / اتصال ورودی‌های پروفایل: با تغییر، POST /s؛ نشانگر کنار فیلد مقدار «اعمال‌شده» را از STM32 نشان می‌دهد. v1.14d: مقدار ناسازگار قبل از ارسال تأیید می‌خواهد چون برد گیره‌اش می‌زند. */
for(const id of [7,8,20,21,22,23,24,25,26]){const e=$('q'+id);if(!e)continue;e.onchange=()=>{const v=parseInt(e.value,10);if(isNaN(v))return;
 if(id>=20){const m=qchk().filter(x=>x.ids.includes(id));
  if(m.length&&!confirm('⚠ '+m.map(x=>x.msg).join('\n')+'\n\nبرد مقدار را گیره می‌زند تا مجموعه سازنده بماند. باز هم ارسال شود؟')){e.value='';qgraph();return;}}
 send(id,v);};if(id>=20)e.oninput=qgraph;}
/* ===== v1.15: تب آلارم‌ها — آینهٔ قوانین Fault_ClampAlarms/Charger_ClampAlarms روی برد ===== */
const AIDS=[];for(let _i=27;_i<=76;_i++)AIDS.push(_i);
const ADEF=[14800,150,6000,7000,1000,1000,21000,28000,950,15000,2000,1000,50,10000,1000,1,0,1000,50,3000,233,3,100,40,20,10,1,60000,20000,10000,100,1,1000,2000,10000,1,2,3,100,1000,10,1000,10,28000,1000,21000,21200,21000,29000,0];
function av(id){const e=$('q'+id),d=D&&D.p&&D.p[id]!=null?D.p[id]:ADEF[id-27];
 if(e&&e.value!==''){const v=parseInt(e.value,10);if(!isNaN(v))return{v,d};}
 return{v:d,d};}
function ap(){
 const g=(id,fb)=>D&&D.p&&D.p[id]!=null?D.p[id]:fb;
 /* [EN] profile refs come from the APPLIED board values (QDEF fallback); alarm refs from typed-or-applied (av). */
 const o={over:g(22,14600),imax:g(25,650),d:av(27).v,dd:av(28).v,ab:av(29).v,bk:av(30).v,ad:av(31).v,rc:av(32).v,mn:av(33).v,mx:av(34).v,hd:av(35).v,ov:av(36).v,fl:av(37).v};
 AIDS.forEach(id=>{o['u'+id]=av(id).v;});
 return o;}
function achk(){const a=ap(),w=[],bad=(v,lo,hi)=>!(v>=lo&&v<=hi);
 const dlo=Math.max(14000,a.over+50),dhi=Math.min(15000,a.ov-100);
 if(dlo>dhi)w.push({ids:[27,36],msg:'بازهٔ قطع خالی است — قطع OV را بالا ببرید یا سقف تجاوز را پایین بیاورید'});
 else if(bad(a.d,dlo,dhi))w.push({ids:[27],msg:'قطع باتری باید '+dlo+'..'+dhi+' باشد (بالای تجاوز+۵۰، زیر OV−۱۰۰)'});
 if(bad(a.dd,50,1000))w.push({ids:[28],msg:'دبانس قطع باید ۵۰..۱۰۰۰ باشد'});
 if(bad(a.ab,3000,8000))w.push({ids:[29],msg:'غیبت باید ۳۰۰۰..۸۰۰۰ باشد'});
 else if(!(a.ab<=a.bk-500))w.push({ids:[29,30],msg:'غیبت باید زیر بازگشت−۵۰۰ باشد (≤ '+(a.bk-500)+')'});
 if(bad(a.bk,4000,9000))w.push({ids:[30],msg:'برگشت باید ۴۰۰۰..۹۰۰۰ باشد'});
 else if(!(a.bk>=a.ab+500))w.push({ids:[30,29],msg:'برگشت باید بالای غیبت+۵۰۰ باشد (≥ '+(a.ab+500)+')'});
 if(bad(a.ad,100,5000))w.push({ids:[31],msg:'دبانس غیبت باید ۱۰۰..۵۰۰۰ باشد'});
 if(bad(a.rc,100,5000))w.push({ids:[32],msg:'دبانس بازیابی باید ۱۰۰..۵۰۰۰ باشد'});
 if(bad(a.mn,18000,24000))w.push({ids:[33],msg:'کف ورودی باید ۱۸۰۰۰..۲۴۰۰۰ باشد'});
 else if(!(a.mn<=a.mx-1000))w.push({ids:[33,34],msg:'کف ورودی باید زیر سقف−۱۰۰۰ باشد (≤ '+(a.mx-1000)+')'});
 if(bad(a.mx,24000,30000))w.push({ids:[34],msg:'سقف ورودی باید ۲۴۰۰۰..۳۰۰۰۰ باشد'});
 else if(!(a.mx>=a.mn+1000))w.push({ids:[34,33],msg:'سقف ورودی باید بالای کف+۱۰۰۰ باشد (≥ '+(a.mn+1000)+')'});
 if(bad(a.hd,a.imax+50,950))w.push({ids:[35,25],msg:'خطای سخت باید '+(a.imax+50)+'..۹۵۰ باشد (بالای بالک+۵۰، هرگز بالای ۹۵۰)'});
 const olo=Math.max(14000,a.over+150);
 if(bad(a.ov,olo,15000))w.push({ids:[36,22],msg:'قطع OV باید '+olo+'..۱۵۰۰۰ باشد (بالای تجاوز+۱۵۰، هرگز بالای ۱۵۰۰۰)'});
 if(bad(a.fl,0,8000))w.push({ids:[37],msg:'کف اعتبار باید ۰..۸۰۰۰ باشد'});
 /* v1.16: نگهبان اعداد UI (آینهٔ Ui_ClampAlarms) */
 const perOk=v=>v===0||(v>=1000&&v<=600000);
 const fit=(dur,per,cnt,gap)=>per===0||dur*cnt+(cnt>1?gap*(cnt-1):0)<=per;
 const perW=(id,v,nm)=>{if(!(v===0||(v>=1000&&v<=600000)))w.push({ids:[id],msg:nm+' باید صفر (خاموش) یا ۱۰۰۰..۶۰۰۰۰۰ باشد'});};
 [[38,'دوره چشمک قرمز OV',100,10000],[39,'دیوتی قرمز OV',0,100],[42,'تعداد بوق OV',0,10],[43,'گپ بوق OV',0,5000],
  [44,'دوره چشمک قرمز قطع باتری',100,10000],[45,'دیوتی قرمز قطع باتری',0,100],[48,'تعداد بوق قطع باتری',0,10],[49,'گپ بوق قطع باتری',0,5000]].forEach(x=>{if(bad(a['u'+x[0]],x[2],x[3]))w.push({ids:[x[0]],msg:x[1]+' باید '+x[2]+'..'+x[3]+' باشد'});});
 perW(40,a.u40,'دوره بوق OV');perW(46,a.u46,'دوره بوق قطع باتری');
 if(bad(a.u41,0,600000))w.push({ids:[41],msg:'مدت هر بوق OV باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u41,a.u40,a.u42,a.u43))w.push({ids:[41,40,42,43],msg:'بوق OV در دوره جا نمی‌شود (مدت×تعداد+گپ‌ها ≤ دوره) — برد بی‌صدا می‌ماند'});
 if(a.u42>1&&a.u40!==0&&a.u43<100)w.push({ids:[43],msg:'گپ بوق OV با چند بوق باید دست‌کم ۱۰۰ باشد'});
 if(bad(a.u47,0,600000))w.push({ids:[47],msg:'مدت هر بوق قطع باتری باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u47,a.u46,a.u48,a.u49))w.push({ids:[47,46,48,49],msg:'بوق قطع باتری در دوره جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(a.u48>1&&a.u46!==0&&a.u49<100)w.push({ids:[49],msg:'گپ بوق قطع باتری با چند بوق باید دست‌کم ۱۰۰ باشد'});
 [[50,'شروع بوق'],[51,'باند دو-بوق'],[52,'باند سه-بوق'],[53,'باند بحرانی']].forEach(x=>{if(bad(a['u'+x[0]],0,100))w.push({ids:[x[0]],msg:x[1]+' باید ۰..۱۰۰ باشد'});});
 if(!(a.u51<=a.u50))w.push({ids:[51,50],msg:'باند دو-بوق باید زیر شروع بوق باشد (≤ '+a.u50+')'});
 if(!(a.u52<=a.u51))w.push({ids:[52,51],msg:'باند سه-بوق باید زیر باند دو-بوق باشد (≤ '+a.u51+')'});
 if(!(a.u53<=a.u52))w.push({ids:[53,52],msg:'باند بحرانی باید زیر باند سه-بوق باشد (≤ '+a.u52+')'});
 perW(54,a.u54,'فاصله بوق ۱/۲تایی');perW(55,a.u55,'فاصله بوق ۳تایی');perW(56,a.u56,'دوره بوق بحرانی');
 if(bad(a.u57,0,100))w.push({ids:[57],msg:'دیوتی بوق بحرانی باید ۰..۱۰۰ باشد'});
 [[58,'تعداد بوق بحرانی'],[62,'تعداد بوق باند ۱'],[63,'تعداد بوق باند ۲'],[64,'تعداد بوق باند ۳']].forEach(x=>{if(bad(a['u'+x[0]],0,10))w.push({ids:[x[0]],msg:x[1]+' باید ۰..۱۰ باشد'});});
 if(bad(a.u65,0,5000))w.push({ids:[65],msg:'گپ بوق دشارژ باید ۰..۵۰۰۰ باشد'});
 else if((a.u58>1||a.u62>1||a.u63>1||a.u64>1)&&a.u65<100)w.push({ids:[65],msg:'گپ بوق دشارژ با چند بوق باید دست‌کم ۱۰۰ باشد'});
 if(bad(a.u59,0,600000))w.push({ids:[59],msg:'مدت هر بوق ۱/۲تایی باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u59,a.u54,Math.max(a.u62,a.u63),a.u65))w.push({ids:[59,54,62,63,65],msg:'بوق ۱/۲تایی دشارژ در فاصله جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(bad(a.u60,0,600000))w.push({ids:[60],msg:'مدت هر بوق ۳تایی باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u60,a.u55,a.u64,a.u65))w.push({ids:[60,55,64,65],msg:'بوق ۳تایی دشارژ در فاصله جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(bad(a.u61,0,120000))w.push({ids:[61],msg:'طول بوق بحرانی یک‌باره باید ۰..۱۲۰۰۰۰ باشد'});
 {const cw=Math.floor(a.u56*a.u57/100);if(a.u56!==0&&a.u57!==0&&a.u58>1){const cg=a.u65*(a.u58-1);if(cg>=cw||(cw-cg)<a.u58)w.push({ids:[56,57,58,65],msg:'بوق بحرانی در پنجره جا نمی‌شود (گپ‌ها + دست‌کم ۱ms هر بوق ≤ دوره×دیوتی) — برد تعداد را کم می‌کند'});}}
 if(bad(a.u66,100,10000))w.push({ids:[66],msg:'دوره چشمک سبز باید ۱۰۰..۱۰۰۰۰ باشد'});
 if(bad(a.u67,0,10000))w.push({ids:[67],msg:'حداقل خاموشی سبز باید ۰..۱۰۰۰۰ باشد'});
 else if(!(a.u67<=a.u66))w.push({ids:[67,66],msg:'حداقل خاموشی سبز باید زیر دوره باشد (≤ '+a.u66+')'});
 if(bad(a.u68,100,10000))w.push({ids:[68],msg:'دوره چشمک زرد باید ۱۰۰..۱۰۰۰۰ باشد'});
 if(bad(a.u69,0,10000))w.push({ids:[69],msg:'حداقل خاموشی زرد باید ۰..۱۰۰۰۰ باشد'});
 else if(!(a.u69<=a.u68))w.push({ids:[69,68],msg:'حداقل خاموشی زرد باید زیر دوره باشد (≤ '+a.u68+')'});
 if(bad(a.u70,24000,32000))w.push({ids:[70],msg:'آستانه اضافه‌ولتاژ باید ۲۴۰۰۰..۳۲۰۰۰ باشد'});
 if(bad(a.u71,0,2000))w.push({ids:[71],msg:'هیسترزیس اضافه‌ولتاژ باید ۰..۲۰۰۰ باشد'});
 if(bad(a.u72,15000,24000))w.push({ids:[72],msg:'آستانه باتری کم باید ۱۵۰۰۰..۲۴۰۰۰ باشد'});
 else if(!(a.u72<=a.u73))w.push({ids:[72,73],msg:'آستانه باتری کم باید زیر سطح پاک‌شدن باشد (≤ '+a.u73+')'});
 if(bad(a.u73,15000,24000))w.push({ids:[73],msg:'سطح پاک‌شدن باتری کم باید ۱۵۰۰۰..۲۴۰۰۰ باشد'});
 else if(!(a.u73>=a.u72))w.push({ids:[73,72],msg:'سطح پاک‌شدن باید بالای آستانه باشد (≥ '+a.u72+')'});
 if(bad(a.u74,15000,25000))w.push({ids:[74],msg:'کف نگاشت درصد باید ۱۵۰۰۰..۲۵۰۰۰ باشد'});
 else if(!(a.u74<=a.u75-100))w.push({ids:[74,75],msg:'کف نگاشت باید دست‌کم ۱۰۰ زیر سقف باشد (≤ '+(a.u75-100)+')'});
 if(bad(a.u75,25000,32000))w.push({ids:[75],msg:'سقف نگاشت درصد باید ۲۵۰۰۰..۳۲۰۰۰ باشد'});
 else if(!(a.u75>=a.u74+100))w.push({ids:[75,74],msg:'سقف نگاشت باید دست‌کم ۱۰۰ بالای کف باشد (≥ '+(a.u74+100)+')'});
 if(bad(a.u76,0,1))w.push({ids:[76],msg:'میوت باید ۰ یا ۱ باشد'});
 return w;}
function afresh(){const w=achk();
 const wset=(el,l)=>{if(!el)return;el.innerHTML=l.length?('⚠ ترکیب نامعتبر — برد این‌ها را گیره می‌زند: '+l.map(x=>x.msg).join('؛ ')):'';el.style.cssText=l.length?'margin:2px 0 6px;color:#ff7373;font-size:12.5px;line-height:1.9':'margin:2px 0 0';};
 wset($('aw'),w.filter(x=>x.ids.some(i=>i<38)));wset($('aw2'),w.filter(x=>x.ids.some(i=>i>=38)));
 for(const id of AIDS){const ne=$('q'+id);if(ne)ne.style.borderColor=w.some(x=>x.ids.includes(id))?'#b8323f':'';}
 const ms=$('xmuteS');if(ms)ms.textContent=(D&&D.p&&D.p[76]===1)?'🔇 میوت روشن — موقتی، با ریست برد پاک می‌شود؛ LEDها همچنان چشمک می‌زنند':'🔊 بوق روشن';}
function apend(id){if(!D)return 0;return id<32?(D.q&(1<<id)):id<64?(D.q2&(1<<(id-32))):((D.q3||0)&(1<<(id-64)));}
function afill(){if(!D||!D.p)return;for(const id of AIDS){const e=$('q'+id),a=$('a'+id);if(!e)continue;if(document.activeElement!==e&&e.value==='')e.value=D.p[id]==null?'':D.p[id];if(a&&!apend(id))a.textContent=D.p[id]==null?'—':D.p[id];}}
function adef(){ADEF.slice(0,11).forEach((v,k)=>{const id=27+k;$('q'+id).value=v;send(id,v);});afresh();}
function sdef(){AIDS.forEach((id,k)=>{if(id<38)return;const e=$('q'+id);if(e)e.value=ADEF[k];send(id,ADEF[k]);});afresh();}
/* v1.15b: کارت وضعیت گروه‌بندی‌شده — اسکلت یک‌بار ساخته می‌شود و هر poll فقط متن/رنگ به‌روز می‌شود (بدون پر/خالی شدن و چشمک) */
const FEXP=[
 ['خطای ADC','نمونه‌برداری ADC نامعتبر است و اندازه‌گیری‌ها قابل‌اعتماد نیست؛ برد محافظه‌کار می‌شود. سیم‌کشی آنالوگ و تغذیه را بررسی کنید.'],
 ['اضافه‌جریان کانال ۱','جریان کانال ۱ از حد گذشت و کانال متوقف شد؛ باتری/بار کانال ۱ را بررسی و برد را ریست کنید.'],
 ['اضافه‌جریان کانال ۲','جریان کانال ۲ از حد گذشت و کانال متوقف شد؛ باتری/بار کانال ۲ را بررسی و برد را ریست کنید.'],
 ['باتری ضعیف','ولتاژ باتری خیلی پایین است؛ باتری را بررسی/شارژ کنید.'],
 ['خطای جیتر کانال ۱','ناپایداری داخلی نمونه‌برداری کانال ۱؛ اگر ماندگار شد برد را ریست کنید.'],
 ['خطای جیتر کانال ۲','ناپایداری داخلی نمونه‌برداری کانال ۲؛ اگر ماندگار شد برد را ریست کنید.'],
 ['قطع باتری','سیم باتری قطع است یا باتری نیست: یا ولتاژ حین پمپ بالای آستانهٔ قطع (۲۷) رفته یا باتری زیر آستانهٔ غیبت (۲۹) با ورودی سالم دیده شده. سیم‌کشی باتری را بررسی کنید؛ با بازگشت هر دو نیمه بالای آستانهٔ برگشت (۳۰) و پایداری (۳۲)، لچ خودکار پاک می‌شود.']];
let ASB=null;
function astat(){const s=$('ast'),b=$('abars');if(!s||!b||!D||!D.t||!D.p)return;
 const t=D.t,p=D.p;
 const g=(id,fb)=>p[id]!=null?p[id]:fb;
 const vin=t[14],vl=t[17],vh=t[18],i1=t[3],i2=t[10];
 const mn=g(33,21000),mx=g(34,28000),dc=g(27,14800),ab=g(29,6000),hd=g(35,950),ov=g(36,15000),fl=g(37,2000);
 if(!ASB){
  s.innerHTML=`<div class="ag">`+[['ورودی'],['باتری پایین'],['باتری بالا'],['جریان ۱ (بالا)'],['جریان ۲ (پایین)']].map((x,k)=>`<div class="ab" id="asb${k}"><small>${x[0]}</small><b class="n" id="asv${k}">—</b><span class="lb" id="asc${k}">—</span><span class="tg" id="asg${k}">—</span></div>`).join('')+`</div><div class="ab" id="asb5" style="margin-top:8px;min-height:0"><small>خطاهای قفل‌شده (fault) — LED جدا برای هر بیت</small><div class="leds" style="margin:0 0 6px" id="asfb"><span class="bit" id="asbb0"><i></i><small>ADC</small></span><span class="bit" id="asbb1"><i></i><small>OC1</small></span><span class="bit" id="asbb2"><i></i><small>OC2</small></span><span class="bit" id="asbb3"><i></i><small>باتری</small></span><span class="bit" id="asbb4"><i></i><small>JIT1</small></span><span class="bit" id="asbb5"><i></i><small>JIT2</small></span><span class="bit" id="asbb6"><i></i><small>قطع‌باتری</small></span></div><div class="fx2" id="asf">—</div></div>`;
  b.innerHTML=[0,1,2,3].map(k=>`<div class="lb" id="abc${k}" style="margin-top:8px">—</div><div class="bar"><i id="abf${k}"></i><span id="abm${k}"></span></div>`).join('');
  ASB={box:[0,1,2,3,4,5].map(k=>$('asb'+k)),val:[0,1,2,3,4].map(k=>$('asv'+k)),cap:[0,1,2,3,4].map(k=>$('asc'+k)),pill:[0,1,2,3,4].map(k=>$('asg'+k)),flt:$('asf'),bits:[0,1,2,3,4,5,6].map(k=>$('asbb'+k)),bcap:[$('abc0'),$('abc1'),$('abc2'),$('abc3')],bfill:[$('abf0'),$('abf1'),$('abf2'),$('abf3')],bmark:[$('abm0'),$('abm1'),$('abm2'),$('abm3')],sig:'',mask:-1};
  if(!ASB.box[0]||!ASB.bfill[0]||!ASB.flt){ASB=null;return;}
 }
 const set=(k,val,cap,pill,cls)=>{ASB.val[k].textContent=val;ASB.cap[k].textContent=cap;ASB.pill[k].textContent=pill;ASB.pill[k].className='tg '+cls;ASB.box[k].className='ab '+(cls==='g'?'good':cls==='y'?'warn':'bad');};
 const vinOk=vin>=mn&&vin<=mx;
 set(0,(vin/1000).toFixed(2)+'V',`بازهٔ سالم ${(mn/1000).toFixed(1)}..${(mx/1000).toFixed(1)}V`,vinOk?'✅ داخل بازه':'⚠ خارج بازه',vinOk?'g':'r');
 [[vl,1],[vh,2]].forEach(B=>{const v=B[0],over=v>=dc,lost=v<ab,inv=v<fl||v>=ov,bad=over||lost||inv;
  set(B[1],(v/1000).toFixed(2)+'V',`قطع ${(dc/1000).toFixed(2)}V · حاشیه ${dc-v}mV`,bad?(over?'⚠ بالای قطع':lost?'⚠ غایب':'⚠ نامعتبر'):'✅ سالم',bad?'r':'g');});
 [[i1,3],[i2,4]].forEach(C=>{const v=C[0];set(C[1],v+'mA',`خطای سخت ${hd}mA`,v>=hd?'⚠ تریپ':v>=hd-100?'⚠ نزدیک تریپ':'✅ سالم',v>=hd?'r':v>=hd-100?'y':'g');});
 if(t[19]!==ASB.mask){ASB.mask=t[19];
  if(!t[19]){ASB.flt.textContent='✅ بدون خطای قفل‌شده';ASB.box[5].className='ab good';}
  else{let h='';for(let bit=0;bit<7;bit++)if(t[19]&(1<<bit))h+=`<div>⚠ <b>${FEXP[bit][0]}</b> — ${FEXP[bit][1]}</div>`;
   if(t[19]&~127)h+=`<div>⚠ بیت ناشناخته: <span class="n">fault 0x${t[19].toString(16)}</span></div>`;
   ASB.flt.innerHTML=h;ASB.box[5].className='ab bad';}}
 const pc2=(x,lo,hi)=>Math.max(0,Math.min(100,(x-lo)/(hi-lo)*100));
 const R=[[vin,Math.max(15000,mn-3000),Math.min(32000,mx+3000),[[mn,'#2ecc8f'],[mx,'#ff5c5c']],`ورودی — سبز=کف ${(mn/1000).toFixed(1)}V · قرمز=سقف ${(mx/1000).toFixed(1)}V`],
  [Math.max(vl,vh),0,16000,[[ab,'#f5b942'],[g(30,7000),'#2ecc8f'],[dc,'#ff5c5c']],`باتری (بالاترین نیمه) — زرد=غیبت · سبز=برگشت · قرمز=قطع`],
  [i1,0,Math.max(1000,hd+100),[[hd,'#ff5c5c']],`جریان ۱ (باتری بالا) — قرمز=خطای سخت ${hd}mA`],
  [i2,0,Math.max(1000,hd+100),[[hd,'#ff5c5c']],`جریان ۲ (باتری پایین) — قرمز=خطای سخت ${hd}mA`]];
 const sig=[mn,mx,ab,g(30,7000),dc,hd].join(',');
 if(sig!==ASB.sig){ASB.sig=sig;R.forEach((r,k)=>{ASB.bcap[k].textContent=r[4];ASB.bmark[k].innerHTML=r[3].map(m=>`<u style="right:${pc2(m[0],r[1],r[2])}%;background:${m[1]}"></u>`).join('');});}
 R.forEach((r,k)=>{ASB.bfill[k].style.width=pc2(r[0],r[1],r[2])+'%';});}
/* ===== v1.16: آینهٔ LED و بازر برد — همان اولویت Ui_Tick با مقادیر اعمال‌شده؛ چشمک با همان دوره/دیوتی برد (فاز محلی، هم‌سرعت) ===== */
let UV={inP:false,ov:false,bat:false,pct:-1,cpct:-1,full:false,critT:0};
function uview(){
 const R=$('ulR'),Y=$('ulY'),G=$('ulG'),B=$('ulB'),sc=$('uscn'),tm=$('utim');
 if(!R)return;
 const now=performance.now();
 if(ASB&&ASB.bits&&D&&D.t){const m=D.t[19]||0,ph=(now%500)<250;ASB.bits.forEach((e,bit)=>{if(e)e.className='bit'+(((m&(1<<bit))!=0&&ph)?' on':'');});}
 const allOff=why=>{R.className='led r';Y.className='led y';G.className='led g';B.className='bz off';if(sc)sc.textContent=why;if(tm)tm.textContent='—';};
 if(!D||!D.t||!D.p||D.on!=1){allOff(!D?'در انتظار داده…':'لینک قطع است — آینه خاموش');if(ASB&&ASB.bits)ASB.bits.forEach(e=>{if(e)e.className='bit';});return;}
 const t=D.t,pp=D.p;
 const g=(id,fb)=>pp[id]!=null?pp[id]:fb;
 const vin=t[14],vbat=Math.max(t[17],t[18]);
 if(!(vin>0||vbat>0)){allOff('داده نامعتبر — همه خاموش (حالت امن برد)');return;}
 const lo=g(74,21000),hi=g(75,29000);
 let raw=hi>lo?Math.round((Math.min(vbat,hi)-lo)/(hi-lo)*100):0;raw=Math.max(0,Math.min(100,raw));
 if(vin>=21000)UV.inP=true;else if(vin<=20000)UV.inP=false;
 const ovT=g(70,28000),ovH=g(71,1000);
 if(!UV.ov&&vin>ovT)UV.ov=true;else if(UV.ov&&vin<=ovT-ovH)UV.ov=false;
 if(vbat<g(72,21000))UV.bat=true;else if(vbat>=g(73,21200))UV.bat=false;
 if(UV.pct<0)UV.pct=raw;else if(!(UV.pct===0&&raw<2)&&Math.abs(raw-UV.pct)>=2)UV.pct=raw;
 if(UV.cpct<0)UV.cpct=raw;else if(Math.abs(raw-UV.cpct)>=5)UV.cpct=raw;
 if(raw>=100)UV.full=true;else if(raw<95)UV.full=false;
 const mute=g(76,0)===1;
 const blink=(per,onMs)=>per>0&&onMs>0&&(now%per)<onMs;
 const beepNow=(per,dur,cnt,gap)=>{if(!per||!dur||!cnt)return false;const w=dur*cnt+(cnt>1?gap*(cnt-1):0);return w>0&&w<=per&&(now%per)<w;};
 let r=false,y=false,gr=false,bz=false,cap='',tim='—';
 const loW=UV.bat?' · ⚠ باتری کم':'';
 if(UV.ov){
  const per=g(38,1000);
  r=blink(per,Math.floor(per*g(39,50)/100));gr=true;bz=beepNow(g(40,10000),g(41,1000),g(42,1),g(43,0));
  tim='قرمز '+per+'ms/'+g(39,50)+'٪ · '+(g(40,10000)&&g(42,1)?('بوق هر '+g(40,10000)+'ms ('+g(42,1)+'×'+g(41,1000)+'ms)'):'بوق خاموش')+' · سقف '+ovT+'mV';
  cap='⚠ اضافه‌ولتاژ ورودی — قرمز چشمک + بوق'+loW;
 }else if((t[19]&64)!=0){
  const per=g(44,1000);
  r=blink(per,Math.floor(per*g(45,50)/100));gr=true;bz=beepNow(g(46,3000),g(47,233),g(48,3),g(49,100));
  tim='قرمز '+per+'ms/'+g(45,50)+'٪ · '+(g(46,3000)&&g(48,3)?('بوق هر '+g(46,3000)+'ms ('+g(48,3)+'×'+g(47,233)+'ms)'):'بوق خاموش');
  cap='⚠ قطع باتری — قرمز چشمک + بوق'+loW;
 }else if(UV.inP){
  const act=[t[6],t[13]].some(s=>s>=1&&s<=3);
  gr=true;
  tim='سبز ثابت';
  if(UV.full)cap='✅ ورودی وصل · فول — سبز ثابت'+loW;
  else if(!act)cap='✅ ورودی وصل · شارژر بیکار — سبز ثابت'+loW;
  else{
   const st=UV.cpct,per=g(68,1000);
   tim='زرد '+per+'ms · سبز ثابت';
   if(st<=0)y=true;
   else if(st<100){y=blink(per,Math.max(g(69,10),Math.min(per,(100-st)*Math.floor(per/100))));}
   cap='🔋 در حال شارژ '+raw+'٪ — زرد با مانده تا فول'+loW;
  }
 }else{
  const st=UV.pct;
  if(st<g(53,1)){
   if(!UV.critT)UV.critT=now;
   const cp=g(56,10000);
   tim='بوق یک‌باره '+g(61,10000)+'ms · LED خاموش';
   bz=cp>0&&g(58,1)>0&&(now-UV.critT)<g(61,10000)&&(now%cp)<cp*g(57,100)/100;
   cap=(now-UV.critT)<g(61,10000)?'🪫 بحرانی — بوق یک‌bاره (LEDها خاموش)'+loW:'🪫 بحرانی — LEDها خاموش · بوق یک‌بار زده شد'+loW;
  }else{
   UV.critT=0;
   const per=g(66,1000);
   gr=blink(per,per-Math.max(g(67,10),(100-st)*Math.floor(per/100)));
   tim='سبز '+per+'ms';
   let bi='';
   if(st>=g(50,40)){bz=false;tim+=' · بی‌صدا';}
   else if(st>=g(51,20)){bz=beepNow(g(54,60000),g(59,1000),g(62,1),g(65,100));bi=' · بوق تکی';tim+=' · بوق هر '+g(54,60000)+'ms ('+g(62,1)+'×'+g(59,1000)+'ms)';}
   else if(st>=g(52,10)){bz=beepNow(g(54,60000),g(59,1000),g(63,2),g(65,100));bi=' · دو بوق';tim+=' · بوق هر '+g(54,60000)+'ms ('+g(63,2)+'×'+g(59,1000)+'ms)';}
   else{bz=beepNow(g(55,20000),g(60,2000),g(64,3),g(65,100));bi=' · سه بوق';tim+=' · بوق هر '+g(55,20000)+'ms ('+g(64,3)+'×'+g(60,2000)+'ms)';}
   cap='🔋 دشارژ '+st+'٪ — سبز چشمک'+bi+loW;
  }
 }
 R.className='led r'+(r?' on':'');Y.className='led y'+(y?' on':'');G.className='led g'+(gr?' on':'');
 B.className=mute?'bz muted':(bz?'bz':'bz off');
 if(sc)sc.textContent=cap+(mute?' · 🔇 میوت':'');
 if(tm)tm.textContent=tim;
}
function xmute(){const v=(D&&D.p&&D.p[76]===1)?0:1;const f=$('q76');if(f)f.value=v;send(76,v);}
/* اتصال ورودی‌های آلارم (۲۷..۷۶): مثل پروفایل + نگهبان + q2/q3 برای شناسه‌های ۳۲..۷۶ */
for(const id of AIDS){const e=$('q'+id);if(!e)continue;e.onchange=()=>{const v=parseInt(e.value,10);if(isNaN(v))return;
 const m=achk().filter(x=>x.ids.includes(id));
 if(m.length&&!confirm('⚠ '+m.map(x=>x.msg).join('\n')+'\n\nبرد مقدار را گیره می‌زند تا مجموعه سازنده بماند. باز هم ارسال شود؟')){e.value='';afresh();return;}
 send(id,v);};e.oninput=afresh;}
/* ===== v1.15b: پشتیبان‌گیری JSON تنظیمات (فیلتر + پروفایل + آلارم‌ها) ===== */
const XIDS=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,20,21,22,23,24,25,26];AIDS.forEach(id=>{if(id<76)XIDS.push(id);});
function xexp(){const x=$('xst');if(!D||!D.p){if(x)x.textContent='هنوز داده‌ای از برد نرسیده';return;}
 const o={app:'ChangeOver-settings',v:1,params:{}};XIDS.forEach(id=>{o.params[id]=D.p[id];});
 const u=URL.createObjectURL(new Blob([JSON.stringify(o)],{type:'application/json'}));
 const a=document.createElement('a');a.href=u;a.download='changeover-settings.json';a.click();
 setTimeout(()=>URL.revokeObjectURL(u),2000);
 if(x)x.textContent='⬇ خروجی گرفته شد ('+XIDS.filter(id=>D.p[id]!=null).length+' مقدار اعمال‌شده)';}
async function ximp(f){const x=$('xst');let o;try{o=JSON.parse(await f.text());}catch(e){if(x)x.textContent='⚠ فایل JSON معتبر نیست';return;}
 const ps=o&&o.params?o.params:{};
 const jobs=XIDS.filter(id=>Number.isFinite(+ps[id])).map(id=>[id,Math.round(+ps[id])]);
 if(!jobs.length){if(x)x.textContent='⚠ هیچ مقدار معتبری در فایل نیست';return;}
 if(!confirm(jobs.length+' مقدار از فایل روی برد اعمال شود؟\nبرد هر کدام را گیره می‌زند؛ نتیجه کنار همان فیلد دیده می‌شود.'))return;
 let ok=0;for(const j of jobs){try{const r=await fetch('/s?id='+j[0]+'&v='+j[1],{method:'POST'});if(r.ok)ok++;}catch(e){}if(x)x.textContent='… '+ok+'/'+jobs.length;await sl(130);}
 if(x)x.textContent=(ok===jobs.length?'✅ ':'⚠ ')+ok+'/'+jobs.length+' اعمال شد — مقادیر گیره‌خورده کنار فیلدها';
 const xi=$('xim');if(xi)xi.value='';}
$('xim').onchange=e=>{if(e.target.files[0])ximp(e.target.files[0]);};
function draw(d){D=d;const t=d.t,p=d.p,on=d.on==1,man=(d.fl&32)!=0;qfill();afill();if(TAB==2){if(STAB==0)qgraph();else afresh();}astat();
 document.body.classList.toggle('dn',!on);$('lk').classList.toggle('on',on);
 $('lt').innerHTML=on?`آنلاین · <span class="n">seq ${d.seq}</span>`:(d.n?'لینک قطع است':'در انتظار STM32…');
 hist(d);
 V.forEach((v,i)=>$('v'+i).textContent=v2(t[v[1]]));
 const F=[['snapshot',d.fl&1],['ورودی ۲۴V',d.fl&2],['اندازه‌گیری معتبر',d.fl&4]];
 $('fl').innerHTML=F.map(f=>`<span class="tg ${f[1]?'g':'r'}">${f[0]}</span>`).join('')+(t[19]&64?'<span class="tg r">خطا: باتری قطع</span>':'')+
  (t[19]&~64?`<span class="tg r n">fault 0x${t[19].toString(16)}</span>`:'')+(man?'<span class="tg y">مود دستی</span>':'');
 [1,2].forEach(n=>{const b=n==1?0:7,s=t[b+6],en=p[10+n],ce=p[12+n],fx=p[13+2*n];
  const st=$('st'+n);st.textContent=(ST[s]||'#'+s)+(fx===1&&!man?' · فیکس':'');st.className='tg '+(SC[s]||'');
  $('ie'+n).innerHTML=t[b+4]+' <span class="lb">mA</span>';$('du'+n).textContent=pc(t[b+5]);$('dc'+n).textContent=ce==null?'':'· سقف '+pc(ce);
  $('db'+n).style.width=Math.min(100,t[b+5]/10)+'%';$('cl'+n).style.left=(100-Math.min(100,(ce==null?1000:ce)/10))+'%';
  [0,1,2,3,4].forEach(k=>$('c'+n+k).textContent=t[b+k]);
  const g=$('tg'+n);g.textContent=en===0?'وصل مجدد شارژر '+n:'قطع شارژر '+n;g.className='bt '+(en===0?'run':'cut');
});
 for(let id=0;id<20;id++){const a=$('a'+id);if(a&&!(d.q&(1<<id)))a.textContent=p[id]==null?'—':p[id];}
 const fe=$('fspan');if(fe){const mn=p[7]==null?null:(p[7]>=3?p[7]:0),av=p[8]==null?null:(p[8]>=2?p[8]:0);
  fe.innerHTML=(mn==null||av==null)?'—':'فیلتر فعال: مدین '+(p[7]>=3?p[7]+'×1ms':'خاموش (۱..۲)')+' + میانگین '+(p[8]>=2?p[8]+'×1ms':'خاموش (۱)')+' ≈ <b>'+((mn||0)+(av||0))+'ms</b> تاریخچه در کادانس ۱kHz — پنل هر ۱۰۰ms فریم TLM می‌گیرد؛ برای صاف‌شدنِ قابل‌مشاهده مجموع را بالای ~۲۰۰ms ببرید (در مود خودکار ≤۵۰).';}
 formulas(t,p);chart();mview(d);
 $('mb').classList.toggle('v',man);$('ka').innerHTML=man?(d.ka<1500?`پایش لینک فعال · <span class="n">keepalive ${d.ka} ms</span>`:'<b>keepalive متوقف است</b>'):'';}
async function poll(){const c=new AbortController(),k=setTimeout(()=>c.abort(),2000);try{const r=await fetch('/t',{cache:'no-store',signal:c.signal});const d=await r.json();clearTimeout(k);if(document.hidden){D=d;hist(d);}else draw(d);}catch(e){clearTimeout(k);document.body.classList.add('dn');$('lk').classList.remove('on');$('lt').textContent='ESP در دسترس نیست';}
 setTimeout(poll,300);}
/* ---------- ابزار بنچ (بخش 5.5 و 5.6 نسخه ۲؛ همه دستی، هیچ ضریبی خودکار ارسال نمی‌شود) ----------
 * Bench tools (spec 5.5 / 5.6 v2): manual only; no coefficient is ever sent automatically. */
const gv=id=>{const e=$(id);if(!e||e.value==='')return null;const v=+e.value;return isNaN(v)?null:v;};
const fa=n=>'۱۲'[n-1],r0=Math.round;
/* ذخیرهٔ خودکار ورودی‌ها در همین مرورگر (با بستن صفحه پاک نمی‌شوند) */
let BDS={};try{BDS=JSON.parse(localStorage.getItem('bd')||'{}');}catch(e){}
function bsave(){document.querySelectorAll('[data-s]').forEach(e=>{if(e.value==='')delete BDS[e.id];else BDS[e.id]=e.value;});try{localStorage.setItem('bd',JSON.stringify(BDS));}catch(e){}}
function bload(r){r.querySelectorAll('[data-s]').forEach(e=>{if(BDS[e.id]!=null)e.value=BDS[e.id];});}
/* ----- داده‌برداری بنچ (بخش 5.6 نسخه ۲): همهٔ مرحله‌ها در یک جدول؛ جلو رفتن فقط با دکمهٔ کاربر -----
 * هر مرحله: duty → صبر → پنجرهٔ /m (قبلش GET_PARAMS) → توقف روی ردیف فعال برای عدد مولتی‌متر → با دکمهٔ ثبت و مرحلهٔ بعد یک ردیف ۸۹ستونی در ESP.
 * Capture v2: set duty → settle → /m window (preceded by GET_PARAMS) → STOP on the active table row for the DMM → one 128-column row only on submit. */
const WSC={SOLO1:[1],SOLO2:[2],BOTH:[1,2]};let W={run:false,abort:false,act:null};
const sl=ms=>new Promise(r=>setTimeout(r,ms));
async function req(u,m,body){const o={method:m||'GET',cache:'no-store'};if(body!=null){o.body=body;o.headers={'Content-Type':'text/plain'};}const r=await fetch(u,o);let j={};try{j=await r.json();}catch(e){}j._s=r.status;return j;}
function wst(m,c){const e=$('wS0');e.innerHTML=m;e.className='cm '+(c||'lb');}
/* ایمنی حین ثبت: لینک، خطای نهایی، JIT، قطع ۱۵V، خاموش شدن ناخواستهٔ مود دستی */
function wchk(){if(W.abort)throw 'پایان توسط کاربر';const d=D;if(!d||d.on!=1)throw 'لینک STM32 قطع شد';
 (W.act||[]).forEach(n=>{const s=d.t[(n-1)*7+6];if(s==7)throw 'خطای نهایی کانال '+fa(n);if(s==5)throw 'تریپ JIT کانال '+fa(n);if(d.t[n==1?18:17]>=15000)throw 'قطع ۱۵V کانال '+fa(n);});
 if(W.man&&!(d.fl&32))throw 'مود دستی قطع شد (ددمن یا محافظ پنل)';}
/* نوشتن پارامتر و صبر تا گزارش همان مقدار از STM32 */
async function setv(id,v){for(let k=0;k<3;k++){const j=await req('/s?id='+id+'&v='+v,'POST');if(j._s!=200)throw 'پاسخ ESP: '+j._s;const e=Date.now()+2500;while(Date.now()<e){await sl(150);if(D&&D.p[id]===v)return;}}throw 'برد مقدار شناسهٔ '+id+' = '+v+' را گزارش نکرد';}
async function wrestore(o){wst('بازگردانی تنظیمات قبل از ثبت…');W.man=false;W.act=null;const L=[[16,0],[18,0],[19,o[19]],[11,o[11]],[12,o[12]]];if(o[19]===0)L.push([16,o[16]],[18,o[18]]);for(const [i,v] of L){try{await setv(i,v);}catch(e){}}}
async function wlog(txt){const j=await req('/benchlog/add','POST',txt);if(j._s==507)throw 'فایل پر است (حدود ۱۰۰KB). فایل را دانلود و پاک کنید.';if(j._s!=200)throw 'نوشتن در فایل انجام نشد (پاسخ '+j._s+')';wfs(j.size);return j;}
function wfs(sz){$('wF').innerHTML=sz==null?'—':`<span class="n">${(sz/1024).toFixed(1)} / 100 KB</span>`;}
async function winfo(){try{const j=await req('/benchlog?i=1');if(j._s!=200)return null;wfs(j.size);if(!j.fs)$('wF').innerHTML='<b class="erc">فایل‌سیستم ESP در دسترس نیست</b>';return j;}catch(e){return null;}}
async function wclear(){if(W.run)return;if(!confirm('فایل ثبت بنچ کامل پاک شود؟ (اول آن را دانلود کنید)'))return;const j=await req('/benchlog/clear','POST');if(j._s==200){try{localStorage.removeItem('wrun');}catch(e){}wst('فایل پاک شد.','cm g');}else wst('پاک کردن انجام نشد.','cm r');winfo();}
const asc=s=>String(s||'').replace(/[^ -~]/g,'').replace(/,/g,';').trim().slice(0,80);
function wlist(){const a=$('wL').value.split(/[,، ]+/).filter(x=>x!=='').map(Number);if(!a.length||a.some(x=>!(x>=0&&x<=50)))throw 'فهرست duty نامعتبر است (درصد بین ۰ و ۵۰، با کاما جدا؛ مثلاً 5,10,15,20)';return a;}
/* پنجرهٔ /m: هر ۲۰ فیلد t[] با مجموع/کمینه/بیشینه/آخرین فریم، OR خطاها، seq و flags آخر.
 * باز شدن فرم مولتی‌متر پنجره را صفر می‌کند و همان لحظهٔ زدن «ثبت» خوانده می‌شود — آمار مال لحظهٔ عدد دادن شماست، نه قبلش (دستور کاربر ۲۰۲۶-۰۹-۲۵).
 * Opening the DMM form resets the window; the submit press reads it: the stats belong to the moment you press, not before. */
async function wopen(){const j=await req('/m','POST');if(j._s!=200)throw 'پنجرهٔ آمار ESP پاسخ نداد';W.winMs=Date.now();}
async function wlatch(){const j=await req('/m');if(!j.n||!j.s||j.s.length<20)throw 'در این بازه TLM نرسید';j.a=i=>j.s[i]/j.n;return j;}
/* خانه‌های زندهٔ ردیف فعال از آخرین /t — فقط نمایش؛ ردیف فایل از /m لحظهٔ ثبت ساخته می‌شود */
function wlive(act){if(!D||D.on!=1)return['-','-','-',undefined,'-','-','-',undefined,undefined];const M=(n,b)=>act.includes(n)?[D.t[b],D.t[b+3],D.t[b+4]]:['قطع','-','-'];const a=M(1,0),b=M(2,7);return[a[0],a[1],a[2],undefined,b[0],b[1],b[2],undefined,undefined];}
/* ردیف CSV (۱۲۸ ستون، ترتیب دقیق بخش 5.6، مولتی‌متر نسخه ۴؛ v1.16: +۳۹ ستون UI) / CSV row, exact 5.6 column order (DMM v4; v1.16: +39 UI cols) */
function wrow(sc,i,pm,se,sa,m,v,iso){const q=x=>x==null?'-':x,P=[];for(let k=0;k<77;k++)P.push(q(D.p[k]));
 const C=b=>[m.a(b).toFixed(1),m.lo[b],m.hi[b],r0(m.a(b+1)),r0(m.a(b+2)),m.lo[b+2],m.hi[b+2],r0(m.a(b+3)),m.lo[b+3],m.hi[b+3],r0(m.a(b+4)),m.lo[b+4],m.hi[b+4],m.la[b+5],m.la[b+6]];
 return [sc,i+1,pm,se,sa,iso,...P,...C(0),...C(7),m.seq,m.fl,...[14,15,16,17,18].map(k=>r0(m.a(k))),m.or,q(v.ii),q(v.vi),q(v.b1),q(v.v1),q(v.b2),q(v.v2),v.note||'-'].join(',')+'\n';}
/* جدول واحد: هر مرحلهٔ هر سناریو یک ردیف؛ ردیف فعال ورودی‌ها و دکمه‌ها را دارد */
const WH=['سناریو','#','duty %','raw ۱','filt ۱ mA','iest ۱ mA','جریان باتری ۱ mA','raw ۲','filt ۲ mA','iest ۲ mA','جریان باتری ۲ mA','جریان ورودی کل mA','وضعیت'];
function wbuild(SC,L){W.K=[];SC.forEach(sc=>L.forEach((d,i)=>W.K.push({sc,i,d})));
 $('wT').innerHTML=`<div class="tw"><table class="bt2 wt"><tr>${WH.map(h=>`<th>${h}</th>`).join('')}</tr>${W.K.map((k,x)=>`<tr id="wr${x}"><td>${k.sc}</td><td>${k.i+1}</td><td>${k.d}</td>${'<td>·</td>'.repeat(9)}<td class="lb">در صف</td></tr>`).join('')}</table></div>`;}
function wcell(x,A,st,cl){const r=$('wr'+x);if(!r)return;const c=r.children;A.forEach((v,i)=>{if(v!==undefined)c[3+i].innerHTML=v;});if(st!=null){c[12].textContent=st;c[12].className=cl||'lb';}}
function wmeas(m,act){const M=(n,b)=>act.includes(n)?[m.a(b).toFixed(1),r0(m.a(b+3)),r0(m.a(b+4))]:['قطع','-','-'];const a=M(1,0),b=M(2,7);return [a[0],a[1],a[2],undefined,b[0],b[1],b[2],undefined,undefined];}
/* ردیف فعال (مولتی‌متر نسخه ۴): جریان باتری‌ها و جریان ورودی کل در خانهٔ خودشان؛ ولتاژها، یادداشت و سه دکمه در ردیف زیرش
 * Active row (DMM v4): battery currents + total input current inline; voltages, note and the three buttons in the row below */
function wform(x,act){const r=$('wr'+x);r.classList.add('wa');const N=id=>`<input type="number" step="any" id="${id}" class="wi">`,F=n=>act.includes(n)?N('wB'+n):'-';
 wcell(x,wlive(act).map((v,i)=>i==3?F(1):i==7?F(2):i==8?N('wIi'):v),'منتظر عدد شما','wr');
 const L=(id,t,pre)=>`<label class="lb">${t} <input type="number" step="any" id="${id}" class="wi"${pre!=null?' value="'+pre+'"':''}></label>`;
 let WVI=window.WVI||'';/* [EN] input voltage is quasi-static: carry the last submitted DMM reading into the next step (user order 2026-09-25: no need to retype it every step) / ولتاژ ورودی تقریباً ثابت است: آخرین عدد ثبت‌شده در مرحلهٔ بعد پیش‌پر می‌شود */
 const e=document.createElement('tr');e.id='wX';e.innerHTML=`<td colspan="13"><div class="bctl">${L('wVi','ولتاژ ورودی V',WVI)}${L('wV1','ولتاژ باتری ۱ V')}${L('wV2','ولتاژ باتری ۲ V')}<label class="lb">یادداشت <input type="text" id="wN" class="dl" style="width:150px"></label>
<button class="sb" id="wGo">ثبت و مرحلهٔ بعد</button><button class="sb sb2" id="wRe">تکرار همین مرحله</button><button class="sb stp2" id="wEn">پایان</button></div>
<div class="lb">اجباری: جریان ورودی کل + جریان هر باتری روشن (<b>منفی هم مجاز</b> — تخلیهٔ باتری با شارژر خاموش، مثل بار زنر). جریان باتری باید نزدیک عدد پنل باشد؛ ورودی کل به ولتاژ/جریان باتری وابسته است (فرمول توان: ~۲٫۵ برابر در جریان کم تا ~۰٫۹ برابر در بالای بازه — مصرف ثابت برد در جریان کم برجسته می‌شود). ولتاژها (V) و یادداشت اختیاری.</div></td>`;r.after(e);
 const f=$('wB'+act[0]);if(f)f.focus();
 const lv=setInterval(()=>wcell(x,wlive(act)),400);
 return new Promise(res=>{$('wGo').onclick=()=>{const v={},ok=id=>gv(id);/* v1.9 (user order 2026-09-25): negative currents are VALID - with the charger off the battery itself discharges into other loads (e.g. the zener), the DMM then reads minus */
   v.ii=ok('wIi');if(v.ii==null)return alert('جریان ورودی کل اجباری است.');
   for(const n of act){v['b'+n]=ok('wB'+n);if(v['b'+n]==null)return alert('جریان باتری '+n+' اجباری است (کانال '+n+' روشن است).');}
   [['vi','wVi'],['v1','wV1'],['v2','wV2']].forEach(k=>{const y=gv(k[1]);v[k[0]]=y==null?null:r0(y*1000);});WVI=window.WVI=(v.vi==null)?'':(v.vi/1000);v.note=asc($('wN').value);res({a:'next',v,iso:new Date().toISOString()});};
  $('wRe').onclick=()=>res({a:'repeat'});$('wEn').onclick=()=>res({a:'end'});
  r.onkeydown=e.onkeydown=ev=>{if(ev.key=='Enter'&&ev.target.tagName=='INPUT')$('wGo').click();};
  W.ft=setInterval(()=>{try{wchk();}catch(er){clearInterval(W.ft);res({a:'err',e:er});}},200);}).finally(()=>{clearInterval(W.ft);clearInterval(lv);e.remove();r.classList.remove('wa');r.onkeydown=null;});}
async function wStart(){if(W.run)return;if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');let L;
 try{L=wlist();}catch(e){return alert(e);}
 const SC=Object.keys(WSC).filter(k=>$('wc'+k).checked);if(!SC.length)return alert('حداقل یک سناریو را انتخاب کنید.');
 const o={};[11,12,16,18,19].forEach(i=>o[i]=D.p[i]);if(Object.values(o).some(v=>v==null))return alert('پارامترها هنوز از STM32 خوانده نشده‌اند.');
 if(D.p[15]===1||D.p[17]===1)return alert('مود duty فیکس (۱۵/۱۷) روشن است؛ اول خاموشش کنید.');
 const fi=await winfo();if(!fi||!fi.fs)return alert('فایل‌سیستم ESP در دسترس نیست؛ در Arduino IDE چیدمان فلش دارای FS را انتخاب و دوباره فلش کنید.');
 if(!confirm('داده‌برداری شروع شود؟ '+SC.join('، ')+'\nپنل duty هر مرحله را می‌گذارد و منتظر عدد مولتی‌متر شما می‌ماند. آخر هر سناریو تنظیمات قبلی برمی‌گردد.'))return;
 W={run:true,abort:false,act:null,man:false};document.body.classList.add('br');wbuild(SC,L);$('wDone').classList.remove('v');let err=null,x=0;
 try{for(const sc of SC){const act=WSC[sc];
   let run=1;try{run=(+localStorage.getItem('wrun')||0)+1;localStorage.setItem('wrun',run);}catch(e){}
   wst(sc+': آماده‌سازی (duty صفر، قطع/وصل کانال‌ها، مود دستی)…');W.act=null;W.man=false;
   await setv(16,0);await setv(18,0);for(const n of [1,2])await setv(10+n,act.includes(n)?1:0);
   await setv(19,1);{const e=Date.now()+3000;while(!(D.fl&32)){if(Date.now()>e)throw 'مود دستی روشن نشد';await sl(100);}}W.man=true;W.act=act;
   await wlog(`# run ${run} browser_ts=${new Date().toISOString()} scenario=${sc} duty_list=${L.join(';')}\n`);
   try{for(let i=0;i<L.length;){const pm=r0(L[i]*10),lb=sc+' · مرحلهٔ '+(i+1)+' از '+L.length+' · duty '+L[i]+'%';
     wcell(x,[],'در حال اندازه‌گیری','wr');$('wr'+x).scrollIntoView({block:'nearest'});
     for(const n of act){const c=D.p[12+n],v=Math.min(pm,c==null?500:c,500);await setv(14+2*n,v);}
     await wopen();
     wst(lb+': عددهای مولتی‌متر را در ردیف رنگی جدول بنویسید','cm wr');const f=await wform(x,act);
     if(f.a=='err')throw f.e;if(f.a=='end'){W.abort=true;throw 'پایان توسط کاربر';}if(f.a=='repeat'){wcell(x,Array(9).fill('·'),'تکرار');continue;}
     const m=await wlatch();
     await wlog(wrow(sc,i,pm,0,r0(Date.now()-W.winMs),m,f.v,f.iso));
     const A=wmeas(m,act);A[3]=f.v.b1??'-';A[7]=f.v.b2??'-';A[8]=f.v.ii;wcell(x,A,'ثبت شد','okc');
     i++;x++;}}
   finally{await wrestore(o);}}}
 catch(e){err=e;}
 W.K.forEach((k,y)=>{const c=$('wr'+y);if(c&&c.children[12].textContent!='ثبت شد')wcell(y,[],'ثبت نشد');});
 W.run=false;W.act=null;W.man=false;document.body.classList.remove('br');
 if(err&&err!=='پایان توسط کاربر')wst('متوقف شد: '+err+' · تنظیمات قبلی برگشت. ردیف‌های ثبت‌شده در فایل مانده‌اند.','cm r');else wst(err?'با دکمهٔ پایان تمام شد؛ تنظیمات قبلی برگشت.':'همهٔ مرحله‌ها ثبت شد؛ تنظیمات قبلی برگشت.','cm g');
 $('wDone').classList.add('v');winfo();}
/* ---------- ساخت تب‌ها ---------- */
/* تب ۱: داده‌برداری بنچ */
$('p1').innerHTML=`<div class="cd"><div class="ds">هر مرحله: پنل duty را می‌گذارد و جدول روی همان ردیف <b>می‌ایستد</b> تا عدد مولتی‌متر را بنویسی و <b>ثبت</b> کنی — آمار همان لحظهٔ ثبت قفل می‌شود. <b>SOLO1</b>: کانال ۱ · <b>SOLO2</b>: کانال ۲ · <b>BOTH</b>: هر دو. آمپرمتر: یکی در تغذیهٔ کل برد + سری با سیم شارژ هر باتری روشن. هیچ ضریبی خودکار اعمال نمی‌شود.</div>
<div class="bctl"><label class="lb">duty % <input type="text" id="wL" data-s class="dl" value="2,4,6,8,10,12,14,16,18,20" style="width:160px"></label>
${Object.keys(WSC).map(k=>`<label class="lb"><input type="checkbox" id="wc${k}" checked> ${k}</label>`).join('')}</div>
<div class="bctl"><button class="sb brun" onclick="wStart()">شروع</button><button class="sb stp2 wstop" onclick="W.abort=true">پایان</button><span class="lb">فایل: <b id="wF">—</b></span><a class="sb sb2 lnk" href="/benchlog" download="benchlog.csv">دانلود فایل</a><button class="sb sb2 brun" onclick="wclear()">پاک کردن فایل</button></div>
<div class="cm lb" id="wS0"></div><div id="wT"></div>
<div class="wn gb" id="wDone" style="background:#10301f;color:#bff0d8"><b style="color:var(--ok)">فایل آماده است.</b> <a class="sb lnk" href="/benchlog" download="benchlog.csv">دانلود benchlog.csv</a> <button class="sb sb2" onclick="wclear()">پاک کردن فایل</button></div></div>`;
bload(document.body);document.body.addEventListener('input',bsave);document.body.addEventListener('change',bsave);winfo();
/* ---------- کنترل دستی دیوتی دائمی (دستور کاربر ۲۰۲۶-۰۹-۲۵): کارت در تب «پنل» (از v1.10)؛
 * ---------- قرارداد ایمنی بخش 5.2 اسپک بدون تغییر: ددمن ۱۰ ثانیه، سقف کانال (p13/p14)،
 * ---------- JIT با مسلح مجدد با ارسال دوبارهٔ همان دیوتی. هیچ ضریبی اینجا ارسال نمی‌شود. ---------- */
const manOn=()=>!!(D&&((D.fl&32)||D.p[19]===1));
async function qset(n){if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');
 const v=gv('qm'+n);if(v==null)return alert('عدد دیوتی (٪) را وارد کنید.');if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');
 const lim=(D.p[12+n]==null?500:D.p[12+n]),pm=Math.max(0,Math.min(lim,r0(v*10)));
 if(pm<r0(v*10))alert('دیوتی به سقف کانال ('+(lim/10)+'٪) محدود شد.');
 const man=manOn();let go=man;
 if(!man)go=confirm('مود دستی خاموش است؛ روشن شود و دیوتی اعمال گردد؟\n(لغو = فقط عدد دیوتی ذخیره می‌شود)');
 try{if(go&&!man)await setv(19,1);await setv(14+2*n,pm);$('qm'+n).value='';}catch(e){alert(e);}}
$('s19').onclick=async()=>{if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');
 if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');const man=manOn();
 if(!man&&!confirm('شارژر خودکار و محافظت‌های باتری متوقف می‌شوند و دیوتی را خودتان تعیین می‌کنید. ادامه؟'))return;
 try{await setv(19,man?0:1);}catch(e){alert(e);}};
$('ao').onclick=async()=>{if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');
 if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');try{await setv(16,0);await setv(18,0);}catch(e){alert(e);}};
function mview(d){const b=$('s19');if(!b)return;const man=(d.fl&32)!=0,sup=d.p[19]!=null,pend=(d.q&(1<<19))!=0;
 b.disabled=!sup||d.on!=1;b.textContent=!sup?'—':pend?'…':man?'روشن':'خاموش';b.classList.toggle('on',man);
 $('mq').innerHTML=man?('کانال ۱: دیوتی '+pc(d.t[5])+' · جریان '+d.t[3]+' mA — کانال ۲: دیوتی '+pc(d.t[12])+' · جریان '+d.t[10]+' mA'):'';}
poll();
setInterval(uview,50); /* v1.16: آینهٔ LED با ۵۰ms — چشمک هم‌سرعت برد */
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
 * @param  uint16_t__len        [EN] Payload length, 0..512 bytes, u16 LE on the wire (SET frames use 5) / [FA] طول payload، ۰ تا ۵۱۲ بایت (فریم SET پنج بایت است)
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_WriteFrame(uint8_t uint8_t__type, const uint8_t *uint8_t__ptr_payload, uint16_t uint16_t__len)
{
    uint8_t UINT8_T__A__Frame[ESP_LINK_HEADER_SIZE + ESP_LINK_MAX_PAYLOAD + 1u];
    uint8_t uint8_t__lenLo = (uint8_t)(uint16_t__len & 0xFFu);
    uint8_t uint8_t__lenHi = (uint8_t)((uint16_t__len >> 8) & 0xFFu);
    uint8_t uint8_t__xor = (uint8_t)(uint8_t__type ^ uint8_t__lenLo ^ uint8_t__lenHi);
    uint16_t uint16_t__index;

    if (uint16_t__len > ESP_LINK_MAX_PAYLOAD)
    {
        return;
    }

    UINT8_T__A__Frame[0] = ESP_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = ESP_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = uint8_t__type;
    UINT8_T__A__Frame[3] = uint8_t__lenLo;
    UINT8_T__A__Frame[4] = uint8_t__lenHi;

    for (uint16_t__index = 0u; uint16_t__index < uint16_t__len; uint16_t__index++)
    {
        uint8_t uint8_t__byte = uint8_t__ptr_payload[uint16_t__index];
        UINT8_T__A__Frame[ESP_LINK_HEADER_SIZE + uint16_t__index] = uint8_t__byte;
        uint8_t__xor = (uint8_t)(uint8_t__xor ^ uint8_t__byte);
    }

    uint16_t uint16_t__xorPosition = (uint16_t)(ESP_LINK_HEADER_SIZE + uint16_t__len);
    UINT8_T__A__Frame[uint16_t__xorPosition] = uint8_t__xor;
    uint16_t uint16_t__frameSize = (uint16_t)(uint16_t__xorPosition + 1u);

    (void)Serial.write(UINT8_T__A__Frame, uint16_t__frameSize);
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

    for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        uint32_t uint32_t__value = UINT32_T__G__TlmField[uint8_t__index];
        UINT32_T__G__StatLast[uint8_t__index] = uint32_t__value;
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

    UINT32_T__G__StatFaultOr |= UINT32_T__G__TlmField[ESP_STAT_FAULT_FIELD];
    UINT16_T__G__StatSeq = UINT16_T__G__TlmSeq;
    UINT8_T__G__StatFlags = UINT8_T__G__TlmFlags;
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

    if ((UINT8_T__G__RxType == ESP_MSG_TLM_LIVE) && (UINT16_T__G__RxLen == ESP_LINK_TLM_SIZE))
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
    else if ((UINT8_T__G__RxType == ESP_MSG_PARAM_REPORT) && (UINT16_T__G__RxLen == ESP_LINK_PARAM_ITEM_SIZE))
    {
        func__Esp_StoreParamItem(uint8_t__ptr_payload);
    }
    else if ((UINT8_T__G__RxType == ESP_MSG_PARAMS_BULK) && (UINT16_T__G__RxLen >= 1u))
    {
        /* [EN] v1.16: 77 items need u16 offsets (52 x 5 already overflows u8).
           [FA] نسخه ۱.۱۶: ۷۷ آیتم آفست u16 می‌خواهد. */
        uint8_t uint8_t__count = uint8_t__ptr_payload[0];
        uint16_t uint16_t__item;
        for (uint16_t__item = 0u; uint16_t__item < (uint16_t)uint8_t__count; uint16_t__item++)
        {
            uint16_t uint16_t__offset = (uint16_t)(1u + (uint16_t__item * ESP_LINK_PARAM_ITEM_SIZE));
            uint16_t uint16_t__itemEnd = (uint16_t)(uint16_t__offset + ESP_LINK_PARAM_ITEM_SIZE);
            if (uint16_t__itemEnd > UINT16_T__G__RxLen)
            {
                break;
            }
            func__Esp_StoreParamItem(&uint8_t__ptr_payload[uint16_t__offset]);
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
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_LEN_LO;
            break;

        case ESP_RX_WAIT_LEN_LO:
            UINT16_T__G__RxLen = (uint16_t)uint8_t__byte;
            UINT8_T__G__RxXor = (uint8_t)(UINT8_T__G__RxXor ^ uint8_t__byte);
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_LEN_HI;
            break;

        case ESP_RX_WAIT_LEN_HI:
            UINT16_T__G__RxLen = (uint16_t)(UINT16_T__G__RxLen | ((uint16_t)((uint16_t)uint8_t__byte << 8)));
            UINT8_T__G__RxXor = (uint8_t)(UINT8_T__G__RxXor ^ uint8_t__byte);
            if (UINT16_T__G__RxLen > ESP_LINK_MAX_PAYLOAD)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            }
            else
            {
                UINT16_T__G__RxIndex = 0u;
                ESP_RX_STATE_T__G__RxState = (UINT16_T__G__RxLen == 0u) ? ESP_RX_WAIT_XOR : ESP_RX_WAIT_PAYLOAD;
            }
            break;

        case ESP_RX_WAIT_PAYLOAD:
            UINT8_T__G__RxPayload[UINT16_T__G__RxIndex] = uint8_t__byte;
            UINT16_T__G__RxIndex++;
            UINT8_T__G__RxXor = (uint8_t)(UINT8_T__G__RxXor ^ uint8_t__byte);
            if (UINT16_T__G__RxIndex >= UINT16_T__G__RxLen)
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
 * @brief  [EN] GET /t : compact JSON snapshot {on,age,seq,fl,n,q,q2,q3,ka,t[20],p[77]} (v1.16).
 *              t = TLM u32 fields in spec order (offset 4..80); p = applied params or null.
 *         [FA] مسیر GET /t : خلاصه JSON فشرده {on,age,seq,fl,n,q,q2,q3,ka,t[20],p[77]} (نسخه ۱.۱۶).
 *              t فیلدهای u32 تله‌متری به ترتیب سند (آفست ۴ تا ۸۰)؛ p مقدار اعمال‌شده یا null.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpTelemetry(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__ageMs = uint32_t__nowMs - UINT32_T__G__LastTlmMs;
    bool bool__online = BOOL__G__TlmSeen && (uint32_t__ageMs <= ESP_LINK_TIMEOUT_MS);
    uint32_t uint32_t__pendingMask = 0u;
    uint32_t uint32_t__pendingMask2 = 0u;
    uint32_t uint32_t__pendingMask3 = 0u;
    uint32_t uint32_t__keepaliveAgeMs = uint32_t__nowMs - UINT32_T__G__LastKeepaliveMs;
    uint8_t uint8_t__index;
    size_t size_t__used;

    UINT32_T__G__LastBrowserPollMs = uint32_t__nowMs;
    BOOL__G__BrowserSeen = true;

    /* [EN] v1.16: 77 params need three u32 masks (and 1UL << 32+ is UB),
            so ids 0..31 go to "q", 32..63 to "q2" and 64..76 to "q3"
            (panel apend() reads all three).
       [FA] نسخه ۱.۱۶: ۷۷ پارامتر سه ماسک u32 می‌خواهد (و شیفت ۳۲+ تعریف‌نشده
            است)، پس شناسه‌های ۰..۳۱ در q و ۳۲..۶۳ در q2 و ۶۴..۷۶ در q3 می‌روند. */
    for (uint8_t__index = 0u; uint8_t__index < ESP_PARAM_COUNT; uint8_t__index++)
    {
        if (BOOL__G__TxParamPending[uint8_t__index])
        {
            if (uint8_t__index < 32u)
            {
                uint32_t__pendingMask |= (1UL << uint8_t__index);
            }
            else if (uint8_t__index < 64u)
            {
                uint32_t__pendingMask2 |= (1UL << (uint8_t__index - 32u));
            }
            else
            {
                uint32_t__pendingMask3 |= (1UL << (uint8_t__index - 64u));
            }
        }
    }

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
        "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"n\":%lu,\"q\":%lu,\"q2\":%lu,\"q3\":%lu,\"ka\":%lu,\"t\":[",
        bool__online ? 1u : 0u, (unsigned long)uint32_t__ageMs, (unsigned int)UINT16_T__G__TlmSeq,
        (unsigned int)UINT8_T__G__TlmFlags, (unsigned long)UINT32_T__G__TlmFrameCount,
        (unsigned long)uint32_t__pendingMask, (unsigned long)uint32_t__pendingMask2,
        (unsigned long)uint32_t__pendingMask3,
        (unsigned long)uint32_t__keepaliveAgeMs);

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

    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, "]}");
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

    /* [EN] v1.4: any median size 1..15 is valid (even sizes too), so no rounding here.
       [FA] نسخه ۱.۴: هر اندازهٔ مدین ۱..۱۵ مجاز است (زوج هم)، پس اینجا گرد نمی‌شود. */

    UINT32_T__G__TxParamValue[uint8_t__id] = (uint32_t)int32_t__clamped;
    BOOL__G__TxParamPending[uint8_t__id] = true;
    BOOL__G__ParamUserSet[uint8_t__id] = true;
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/**
 * @brief  [EN] POST /m : restart the bench statistics window and queue one GET_PARAMS (spec 5.6 v2).
 *              v1.7: the wizard resets this when the DMM form OPENS and reads it when the user
 *              PRESSES submit, so the logged window is the moment of the typed meters.
 *         [FA] مسیر POST /m : شروع دوبارهٔ پنجرهٔ آمار بنچ و صف کردن یک GET_PARAMS (بخش 5.6 نسخه ۲).
 *              نسخهٔ ۱.۷: ویزارد با باز شدن فرم مولتی‌متر این را صفر می‌کند و همان لحظهٔ زدن «ثبت»
 *              می‌خواند تا پنجرهٔ ثبتشده هم‌لحظهِ عددهای واردشدهٔ کاربر باشد.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpStatReset(void)
{
    uint8_t uint8_t__index;

    for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        UINT32_T__G__StatSum[uint8_t__index] = 0u;
        UINT32_T__G__StatMin[uint8_t__index] = 0u;
        UINT32_T__G__StatMax[uint8_t__index] = 0u;
        UINT32_T__G__StatLast[uint8_t__index] = 0u;
    }

    UINT32_T__G__StatFaultOr = 0u;
    UINT16_T__G__StatSeq = 0u;
    UINT8_T__G__StatFlags = 0u;
    UINT32_T__G__StatCount = 0u;
    BOOL__G__TxGetPending = true;
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/**
 * @brief  [EN] GET /m : {n, s[20] sums, lo[20], hi[20], la[20] last frame, or faults OR, seq, fl} (browser divides s by n).
 *         [FA] مسیر GET /m : {n، s[20] مجموع، lo[20]، hi[20]، la[20] آخرین فریم، or خطاها، seq، fl} (مرورگر s را بر n تقسیم می‌کند).
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpStatRead(void)
{
    const uint32_t *UINT32_T__A__Table[4] = { UINT32_T__G__StatSum, UINT32_T__G__StatMin, UINT32_T__G__StatMax, UINT32_T__G__StatLast };
    const char *CHAR__A__Key[4] = { "s", "lo", "hi", "la" };
    uint8_t uint8_t__table;
    uint8_t uint8_t__index;
    size_t size_t__used;

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE, "{\"n\":%lu",
                                    (unsigned long)UINT32_T__G__StatCount);

    for (uint8_t__table = 0u; uint8_t__table < 4u; uint8_t__table++)
    {
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                                         ",\"%s\":[", CHAR__A__Key[uint8_t__table]);
        for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
        {
            const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
            size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                                             "%s%lu", char__ptr_sep,
                                             (unsigned long)UINT32_T__A__Table[uint8_t__table][uint8_t__index]);
        }
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, "]");
    }

    (void)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, ",\"or\":%lu,\"seq\":%u,\"fl\":%u}",
                   (unsigned long)UINT32_T__G__StatFaultOr, (unsigned int)UINT16_T__G__StatSeq, (unsigned int)UINT8_T__G__StatFlags);
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/**
 * @brief  [EN] Current size of the bench log file (0 when missing or no file system).
 *         [FA] اندازهٔ فعلی فایل ثبت بنچ (بدون فایل یا فایل‌سیستم = ۰).
 * @return [EN] Size in bytes / [FA] اندازه به بایت
 */
static uint32_t func__Esp_BenchLogSize(void)
{
    uint32_t uint32_t__size = 0u;

    if (BOOL__G__FsOk && LittleFS.exists(ESP_BENCHLOG_PATH))
    {
        File file__log = LittleFS.open(ESP_BENCHLOG_PATH, "r");
        if (file__log)
        {
            uint32_t__size = (uint32_t)file__log.size();
            file__log.close();
        }
    }

    return uint32_t__size;
}

/**
 * @brief  [EN] GET /benchlog : download the CSV (header only when empty); GET /benchlog?i=1 : {fs,size,max}.
 *         [FA] مسیر GET /benchlog : دانلود CSV (خالی = فقط عنوان ستون‌ها)؛ GET /benchlog?i=1 : {fs,size,max}.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpBenchLogGet(void)
{
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");

    if (ESP_WEB_SERVER_T__G__Server.hasArg("i"))
    {
        (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE, "{\"fs\":%u,\"size\":%lu,\"max\":%lu}",
                       BOOL__G__FsOk ? 1u : 0u, (unsigned long)func__Esp_BenchLogSize(),
                       (unsigned long)ESP_BENCHLOG_MAX_BYTES);
        ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
        return;
    }

    ESP_WEB_SERVER_T__G__Server.sendHeader("Content-Disposition", "attachment; filename=benchlog.csv");
    if (func__Esp_BenchLogSize() == 0u)
    {
        ESP_WEB_SERVER_T__G__Server.send(200, "text/csv", ESP_BENCHLOG_HEADER);
        return;
    }

    File file__log = LittleFS.open(ESP_BENCHLOG_PATH, "r");
    if (!file__log)
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "text/plain", "fs");
        return;
    }
    (void)ESP_WEB_SERVER_T__G__Server.streamFile(file__log, "text/csv");
    file__log.close();
}

/**
 * @brief  [EN] POST /benchlog/add (text/plain body): append panel-built CSV line(s).
 *              400 = empty/too long/not newline-terminated/non-printable, 503 = no file system,
 *              507 = the ~100 KB cap would be exceeded (nothing written).
 *         [FA] مسیر POST /benchlog/add (بدنهٔ متنی): افزودن خط(های) CSV ساختهٔ پنل.
 *              400 = خالی/خیلی بلند/بدون خط جدید پایانی/نویسهٔ غیرقابل چاپ، 503 = فایل‌سیستم نیست،
 *              507 = از سقف حدود ۱۰۰KB می‌گذرد (چیزی نوشته نمی‌شود).
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpBenchLogAdd(void)
{
    const String string__body = ESP_WEB_SERVER_T__G__Server.arg("plain");
    const uint32_t uint32_t__len = (uint32_t)string__body.length();
    uint32_t uint32_t__index;
    uint32_t uint32_t__size;
    uint32_t uint32_t__extra;

    if ((uint32_t__len == 0u) || (uint32_t__len > ESP_BENCHLOG_MAX_POST) || (string__body[uint32_t__len - 1u] != '\n'))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }
    for (uint32_t__index = 0u; uint32_t__index < uint32_t__len; uint32_t__index++)
    {
        const char char__c = string__body[uint32_t__index];
        if ((char__c != '\n') && ((char__c < ' ') || (char__c > '~')))
        {
            ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
            return;
        }
    }
    if (!BOOL__G__FsOk)
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0}");
        return;
    }

    uint32_t__size = func__Esp_BenchLogSize();
    uint32_t__extra = (uint32_t__size == 0u) ? (uint32_t)(sizeof(ESP_BENCHLOG_HEADER) - 1u) : 0u;
    if ((uint32_t__size + uint32_t__extra + uint32_t__len) > ESP_BENCHLOG_MAX_BYTES)
    {
        (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE, "{\"ok\":0,\"full\":1,\"size\":%lu}", (unsigned long)uint32_t__size);
        ESP_WEB_SERVER_T__G__Server.send(507, "application/json", CHAR__G__JsonBuffer);
        return;
    }

    File file__log = LittleFS.open(ESP_BENCHLOG_PATH, "a");
    if (!file__log)
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0}");
        return;
    }
    if (uint32_t__extra != 0u)
    {
        (void)file__log.print(ESP_BENCHLOG_HEADER);
    }
    (void)file__log.print(string__body);
    file__log.close();

    (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE, "{\"ok\":1,\"size\":%lu}", (unsigned long)func__Esp_BenchLogSize());
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/**
 * @brief  [EN] POST /benchlog/clear : delete the CSV (the next append recreates it with the header).
 *         [FA] مسیر POST /benchlog/clear : حذف CSV (افزودن بعدی آن را با سطر عنوان از نو می‌سازد).
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpBenchLogClear(void)
{
    if (!BOOL__G__FsOk)
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0}");
        return;
    }
    if (LittleFS.exists(ESP_BENCHLOG_PATH))
    {
        (void)LittleFS.remove(ESP_BENCHLOG_PATH);
    }
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
