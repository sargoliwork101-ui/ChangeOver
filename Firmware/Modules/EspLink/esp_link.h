/**
 * @file    esp_link.h
 * @brief   [EN] ESP-Link: power control, binary command protocol and
 *              telemetry over the USART1 board port (user order
 *              2026-09-22: the ESP panel monitors and retunes the
 *              measurement/calibration controls and can cut/reconnect each
 *              charger module).
 *          [FA] ESP-Link: کنترل تغذیه، پروتکل باینری فرمان و تله‌متری روی
 *              پورت USART1 برد (دستور کاربر ۲۰۲۶-۰۹-۲۲: پنل ESP کنترل‌های
 *              اندازه‌گیری/کالیبراسیون را مانیتور و تنظیم می‌کند و می‌تواند
 *              هر ماژول شارژر را قطع/وصل کند).
 *
 * @note    [EN] Frame format (both directions), little-endian payload:
 *              [0xAA][0x55][type:u8][len:u8][payload:len][xor:u8] where xor
 *              is the XOR of type, len and every payload byte. The engine
 *              resynchronizes on the next 0xAA 0x55 after any error.
 *          [FA] قالب فریم (دو جهته)، payload اندیان کوچک:
 *              [0xAA][0x55][type:u8][len:u8][payload:len][xor:u8] که xor
 *              عبارت از XOR نوع و طول و همهٔ بایت‌های payload است. موتور
 *              بعد از هر خطا روی 0xAA 0x55 بعدی همگام می‌شود.
 */

#ifndef ESP_LINK_H
#define ESP_LINK_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Frame constants / ثابت‌های فریم ==================== */

/* [EN] Start-of-frame bytes and geometry. / [FA] بایت‌های شروع فریم و هندسه. */
#define ESPLINK_SOF_BYTE0             0xAAu
#define ESPLINK_SOF_BYTE1             0x55u
/* [EN] Since v1.16 the length field is u16 little-endian (len_lo +
 *      len_hi) - PARAMS_BULK grows past the old u8 ceiling of 255 (v1.22:
 *      99 params = 1 + 99 x 5 = 496 payload bytes). Frame = AA 55 type
 *      len_lo len_hi payload xor; the xor covers type + both length
 *      bytes + payload. Both boards MUST flash together (a v1.15 parser
 *      reads len_hi as payload).
 * [FA] از v1.16 فیلد طول u16 لیتل‌اندین است (len_lo + len_hi) —
 *      PARAMS_BULK از سقف u8 قبلی رد می‌شود (v1.22: ۹۹ پارامتر = ۴۹۶ بایت
 *      payload). فریم = AA 55 نوع len_lo len_hi و xor روی نوع + دو بایت
 *      طول + payload. هر دو برد باید با هم فلش شوند (پارسر v1.15 یعنی
 *      len_hi را payload می‌خواند). */
/* [EN] v2 FRAME (2026-09-29). Two changes, both because this link is about to
 *      carry a calibration campaign and a silent error there is worse than no
 *      data at all:
 *
 *      1. CRC-16/CCITT-FALSE replaces the XOR-8 check. XOR-8 lets roughly 1 in
 *         256 random corruptions through, and it is blind to ANY even number of
 *         flips in the same bit position - which is exactly the pattern a
 *         switching converter's noise produces on a UART. CRC-16 takes that to
 *         about 1 in 65536 and detects every burst up to 16 bits.
 *      2. An explicit VERSION byte. Before this, flashing one side and not the
 *         other produced no error whatsoever: the receiver simply dropped every
 *         frame whose length it did not expect, and the panel went blank with
 *         nothing to explain it. The version byte turns that failure into a
 *         message the operator can act on.
 *
 *      Wire layout: SOF0 SOF1 VER TYPE LEN_LO LEN_HI [payload] CRC_LO CRC_HI
 *      The CRC covers VER, TYPE, both length bytes and the payload.
 * [FA] فریم نسخهٔ ۲ (۲۰۲۶-۰۹-۲۹). دو تغییر، هر دو چون این لینک قرار است یک
 *      کمپین کالیبراسیون را حمل کند و خطای بی‌صدا آنجا از نبودِ داده هم بدتر است:
 *      ۱. CRC-16/CCITT-FALSE جای XOR-8 را می‌گیرد. XOR-8 تقریباً یک از ۲۵۶ خرابی
 *         تصادفی را رد می‌کند و نسبت به هر تعداد زوجِ تغییرِ بیت در یک موقعیت
 *         کاملاً کور است - دقیقاً الگویی که نویز مبدل کلیدزن روی UART می‌سازد.
 *      ۲. بایت نسخهٔ صریح. پیش از این، فلش‌کردن یک طرف و نکردن طرف دیگر هیچ خطایی
 *         تولید نمی‌کرد و پنل بدون هیچ توضیحی خالی می‌ماند. */
#define ESPLINK_PROTOCOL_VERSION      2u
#define ESPLINK_FRAME_HEADER_SIZE     6u   /* SOF0 + SOF1 + ver + type + len_lo + len_hi */
#define ESPLINK_FRAME_CHECKSUM_SIZE   2u   /* CRC-16/CCITT-FALSE, little endian */
#define ESPLINK_CRC16_INIT            0xFFFFu
#define ESPLINK_CRC16_POLY            0x1021u
#define ESPLINK_FRAME_MAX_PAYLOAD     512u

/* [EN] Message types. ESP -> STM: SET_PARAM / GET_PARAMS / CAL_REFERENCE
 *      (v1.3). STM -> ESP: TLM_LIVE (periodic), PARAM_REPORT (after each
 *      SET and as the CAL_REFERENCE reply), PARAMS_BULK (answer to GET).
 *      Unknown types are dropped silently.
 * [FA] انواع پیام. ESP به STM: SET_PARAM / GET_PARAMS / CAL_REFERENCE
 *      (v1.3). STM به ESP: TLM_LIVE (دوره‌ای)، PARAM_REPORT (بعد از هر SET
 *      و به‌عنوان پاسخ CAL_REFERENCE)، PARAMS_BULK (پاسخ GET). نوع
 *      ناشناخته و طول payload غلط بی‌صدا کنار گذاشته می‌شود. */
#define ESPLINK_MSG_SET_PARAM         0x01u
#define ESPLINK_MSG_GET_PARAMS        0x02u
/* [EN] RESERVED, no longer implemented. The panel stopped sending this in
   v1.7 and the STM32 handler was carried dead ever since; it was removed in
   v2 to free flash (the 62 K budget overflowed by 92 bytes). The id stays
   reserved so it is never reused for something else - an older panel that
   still sent 0x03 would then be silently misinterpreted instead of ignored.
   [FA] رزرو، دیگر پیاده‌سازی نشده. پنل از نسخهٔ ۱.۷ دیگر این را نمی‌فرستد و
   هندلر مرده در فرم‌ور مانده بود؛ در نسخهٔ ۲ برای آزادکردن فلش حذف شد. شناسه
   رزرو می‌ماند تا هرگز برای چیز دیگری استفاده نشود. */
#define ESPLINK_MSG_CAL_REFERENCE     0x03u  /* reserved, not handled */

/* [EN] CAL_REFERENCE (0x03) was a v1.3 one-shot bench calibration command.
 *      The panel stopped sending it in the v1.7 simplification, and the STM32
 *      handler was then carried dead for six versions - 172 source lines of a
 *      62 KB flash budget. It is what pushed the v2 build 92 bytes over, so it
 *      was removed. Calibration is done the ordinary way now: read the raw ADC
 *      counts the telemetry publishes, work the coefficients out against a DMM,
 *      and write them back as normal SET_PARAM values.
 *      The id remains RESERVED. Freeing it would be worse than wasting it: if
 *      0x03 were later reused, an older panel still sending CAL_REFERENCE would
 *      be silently misinterpreted instead of harmlessly ignored.
 * [FA] CAL_REFERENCE (0x03) فرمان کالیبراسیون یک‌مرحله‌ای نسخهٔ ۱.۳ بود. پنل از
 *      نسخهٔ ۱.۷ دیگر آن را نمی‌فرستد و هندلرش شش نسخه مرده حمل شد - ۱۷۲ خط از
 *      بودجهٔ ۶۲ کیلوبایتی فلش - و همان چیزی بود که بیلد نسخهٔ ۲ را ۹۲ بایت سرریز
 *      کرد، پس حذف شد. کالیبراسیون حالا به روش عادی انجام می‌شود: شمارش خام ADC
 *      که تله‌متری منتشر می‌کند خوانده می‌شود، ضرایب در برابر مولتی‌متر حساب
 *      می‌شوند و با SET_PARAM معمولی برمی‌گردند.
 *      شناسه رزرو می‌ماند: آزادکردنش از هدردادنش بدتر است، چون پنل قدیمی‌ای که
 *      هنوز ۰x۰۳ می‌فرستد به‌جای نادیده‌گرفته‌شدن، بد تفسیر می‌شود. */
#define ESPLINK_MSG_TLM_LIVE          0x10u
#define ESPLINK_MSG_PARAM_REPORT      0x11u
#define ESPLINK_MSG_PARAMS_BULK       0x12u

/* ==================== Parameter IDs / شناسهٔ پارامترها ==================== */

/* [EN] SET_PARAM payload = [id:u8][value:u32 LE]. Every value is clamped
 *      by the owning module; PARAM_REPORT returns the APPLIED value.
 *      Voltage offsets are signed (two's complement in the u32 wire
 *      field). Ids 0..14 + 20..75 are flash-persisted (~1.5 s debounce);
 *      only the transient test modes 15..19 (+76) are RAM-only. Filters
 *      carry ONE size parameter each (any median 1..15, average window
 *      1..300; size 1 = bypass, no separate on/off switch).
 * [FA] payload ی SET_PARAM = [id:u8][value:u32 LE]؛ هر مقدار در ماژول
 *      مالکش گیره می‌شود و PARAM_REPORT مقدارِ اعمال‌شده را برمی‌گرداند.
 *      آفست‌های ولتاژ علامتدارند (متمم دو در فیلد u32). شناسه‌های ۰..۱۴ و
 *      ۲۰..۷۵ روی فلش می‌مانند؛ فقط مودهای گذرا ۱۵..۱۹ (+۷۶) فقط-RAM
 *      هستند. هر فیلتر یک پارامتر اندازه دارد (مدین ۱..۱۵، میانگین
 *      ۱..۳۰۰؛ ۱ = عبور مستقیم). */
#define ESPLINK_PARAM_CUR1_OFFSET_COUNTS   0u   /* u32, counts,   def 8,    0..255    */
#define ESPLINK_PARAM_CUR2_OFFSET_COUNTS   1u   /* u32, counts,   def 8,    0..255    */
#define ESPLINK_PARAM_CUR1_GAIN_PERMILLE   2u   /* u32, permille, def 1046, 100..3000 */
#define ESPLINK_PARAM_CUR2_GAIN_PERMILLE   3u   /* u32, permille, def 1303, 100..3000 */
#define ESPLINK_PARAM_VIN_OFFSET_MV        4u   /* i32, mV,       def 0,    -5000..5000 (v1.10) */
#define ESPLINK_PARAM_V24_OFFSET_MV        5u   /* i32, mV,       def 0,    -5000..5000 (v1.10) */
#define ESPLINK_PARAM_V12_OFFSET_MV        6u   /* i32, mV,       def 0,    -5000..5000 (v1.10) */
#define ESPLINK_PARAM_FILTER_MEDIAN_SIZE   7u   /* u32, samples,  def 3,    1..15 any, 1..2=bypass (v1.4) */
#define ESPLINK_PARAM_FILTER_AVERAGE_WINDOW 8u  /* u32, samples,  def 10,   1..300, 1=bypass (v1.4, u16 since audit 2026-09-26) */
#define ESPLINK_PARAM_CHG_ETA1_PERMILLE    9u   /* u32, permille, def 0,    0..999, 0=identity (v1.3) */
#define ESPLINK_PARAM_CHG_ETA2_PERMILLE   10u   /* u32, permille, def 0,    0..999, 0=identity (v1.3) */
#define ESPLINK_PARAM_CHG1_ENABLE          11u  /* u32, 0/1,      def 1                */
#define ESPLINK_PARAM_CHG2_ENABLE          12u  /* u32, 0/1,      def 1                */
#define ESPLINK_PARAM_CHG1_DUTY_CEILING    13u  /* u32, permille, def 500,  0..500    */
#define ESPLINK_PARAM_CHG2_DUTY_CEILING    14u  /* u32, permille, def 500,  0..500    */
#define ESPLINK_PARAM_CHG1_DUTY_FIXED_ON   15u  /* u32, 0/1,      def 0                */
#define ESPLINK_PARAM_CHG1_DUTY_FIXED_VAL  16u  /* u32, permille, def 0,    0..500    */
#define ESPLINK_PARAM_CHG2_DUTY_FIXED_ON   17u  /* u32, 0/1,      def 0                */
#define ESPLINK_PARAM_CHG2_DUTY_FIXED_VAL  18u  /* u32, permille, def 0,    0..500    */
/* [EN] v1.2 global manual test mode (user order 2026-09-23): 1 = the
 *      automatic charger is suspended, every battery condition bypassed
 *      and each channel driven directly at param 16/18; see
 *      ESP_AGENT_SPEC.md section 5.2 for the full contract (hardware
 *      floor, JIT re-arm, 3 s link dead-man). / [FA] مود تست دستی سراسری
 *      v1.2 (دستور کاربر): ۱ = شارژر خودکار تعلیق، شرط‌های باتری رد و
 *      درایو مستقیم هر کانال با پارامتر ۱۶/۱۸؛ قرارداد کامل در
 *      ESP_AGENT_SPEC.md بخش 5.2 (کف سخت‌افزاری، re-arm ی JIT، ددمن ۳
 *      ثانیه‌ای لینک). */
#define ESPLINK_PARAM_MANUAL_TEST_MODE     19u  /* u32, 0/1,      def 0                */
/* [EN] Charge profile (v1.12, user order 2026-09-25): shared by BOTH
        channels, flash-persisted like every other parameter since v1.14
        (an unreadable record falls back to the old compile-time
        setpoints). Ids MUST equal
        CHG_PROFILE_PARAM_* in charger.h. All values re-clamped as a set
        on every write (see Charger_ClampProfile).
   [FA] پروفایل شارژ (v1.12، دستور کاربر ۲۰۲۶-۰۹-۲۵): مشترک بین هر دو
        کانال، مثل بقیهٔ پارامترها از نسخهٔ ۱.۱۴ روی فلش می‌ماند (رکورد
        ناخوانا به ست‌پوینت‌های کامپایل‌تایم قبلی برمی‌گردد). شناسه‌ها
        باید برابر CHG_PROFILE_PARAM_* در
        charger.h باشند. هر نوشتن، کل مجموعه را دوباره گیره می‌زند. */
#define ESPLINK_PARAM_CHG_PROFILE_ABSORB_MV          20u  /* u32, mV, def 14400, 11000..14600 */
#define ESPLINK_PARAM_CHG_PROFILE_ABSORB_ENTER_MV    21u  /* u32, mV, def 14300, absorb-500..absorb-50 */
#define ESPLINK_PARAM_CHG_PROFILE_ABSORB_OVER_MV     22u  /* u32, mV, def 14600, absorb+100..min(absorb+400,14750) */
#define ESPLINK_PARAM_CHG_PROFILE_FLOAT_MV           23u  /* u32, mV, def 13500, 9000..absorb-300 */
#define ESPLINK_PARAM_CHG_PROFILE_REENTRY_MV         24u  /* u32, mV, def 12800, 8000..float-300 */
#define ESPLINK_PARAM_CHG_PROFILE_BULK_CURRENT_MAX_MA 25u /* u32, mA, def 650,   100..2950 */
#define ESPLINK_PARAM_CHG_PROFILE_TAPER_CURRENT_MA   26u  /* u32, mA, def 50,    10..min(1475,imax) */
/* [EN] Alarms tab (v1.15, user order 2026-09-26): 27..34 live in the Fault
 *      module (ids MUST equal FAULT_ALARM_PARAM_* in fault.h),
 *      35..37 live in the Charger module (ids MUST equal CHG_ALARM_PARAM_*
 *      in charger.h). All values re-clamped as a set on every write.
 * [FA] تب آلارم‌ها (v1.15، دستور کاربر ۲۰۲۶-۰۹-۲۶): ۲۷..۳۴ در ماژول فالت
 *      (شناسه‌ها باید برابر FAULT_ALARM_PARAM_* در fault.h باشند)، ۳۵..۳۷
 *      در ماژول شارژر (برابر CHG_ALARM_PARAM_* در charger.h). هر نوشتن،
 *      کل مجموعه را دوباره گیره می‌زند. */
#define ESPLINK_PARAM_FAULT_ALARM_DISCONNECT_MV      27u  /* u32, mV, def 14800, over+50..OV-100 */
#define ESPLINK_PARAM_FAULT_ALARM_DISCONNECT_DEB_MS  28u  /* u32, ms, def 150,   50..1000 */
#define ESPLINK_PARAM_FAULT_ALARM_ABSENT_MV          29u  /* u32, mV, def 6000,  3000..8000, < back-500 */
#define ESPLINK_PARAM_FAULT_ALARM_BACK_MV            30u  /* u32, mV, def 7000,  4000..9000, > absent+500 */
#define ESPLINK_PARAM_FAULT_ALARM_ABSENT_DEB_MS      31u  /* u32, ms, def 1000,  100..5000 */
#define ESPLINK_PARAM_FAULT_ALARM_RECOVER_DEB_MS     32u  /* u32, ms, def 1000,  100..5000 */
#define ESPLINK_PARAM_FAULT_ALARM_INPUT_MIN_MV       33u  /* u32, mV, def 21000, 18000..24000, < max-1000 */
#define ESPLINK_PARAM_FAULT_ALARM_INPUT_MAX_MV       34u  /* u32, mV, def 28000, 24000..30000, > min+1000 */
#define ESPLINK_PARAM_CHG_ALARM_HARD_CURRENT_MA      35u  /* u32, mA, def 950,   imax+50..3000 */
#define ESPLINK_PARAM_CHG_ALARM_OV_CUTOFF_MV         36u  /* u32, mV, def 14850 = CHG_OV_CUTOFF_DEFAULT_MV (MAX_VALID 15000 - DECIDE_EARLY 150), over+150..15000 (down-only). The comment said 15000 for several releases while the board booted 14850, and the panel believed the comment - so its factory-restore button raised a safety ceiling. */
#define ESPLINK_PARAM_CHG_ALARM_VALID_FLOOR_MV       37u  /* u32, mV, def 2000,  0..8000 */
/* [EN] UI cadence (v1.16, user order 2026-09-26: virtual LEDs with real
 *      blinking, a buzzer icon with a mute cross, every alarm number
 *      editable): 38..76 live in the Ui module (ids MUST equal
 *      UI_ALARM_PARAM_* in ui_led.h). All values re-clamped as a set on
 *      every write. Id 76 (mute) is panel-session only since v1.16b
 *      (RAM, never flashed, cleared on reboot); the one-shot
 *      BoardTest wiring beep ignores it.
 * [FA] اعداد UI (v1.16، دستور کاربر ۲۰۲۶-۰۹-۲۶: LED مجازی با چشمک واقعی،
 *      آیکون بازر با ضربدر میوت، همهٔ اعداد آلارم قابل اصلاح): ۳۸..۷۶ در
 *      ماژول UI (شناسه‌ها باید برابر UI_ALARM_PARAM_* در ui_led.h باشند).
 *      هر نوشتن، کل مجموعه را دوباره گیره می‌زند. ۷۶ (میوت) از v1.16b
 *      فقط جلسه‌ای است (RAM، هرگز فلش نمی‌شود، با ریبوت پاک می‌شود)؛
 *      بوق تست برد آن را نادیده می‌گیرد. */
#define ESPLINK_PARAM_UI_OV_LED_PERIOD_MS     38u  /* u32, ms, def 1000,  100..10000 */
#define ESPLINK_PARAM_UI_OV_LED_DUTY_PCT      39u  /* u32, %,  def 50,    0..100 */
#define ESPLINK_PARAM_UI_OV_BEEP_PERIOD_MS    40u  /* u32, ms, def 10000, 0=off else 1000..600000 */
#define ESPLINK_PARAM_UI_OV_BEEP_DUR_MS       41u  /* u32, ms, def 1000,  per beep, 0..window-fit */
#define ESPLINK_PARAM_UI_OV_BEEP_COUNT        42u  /* u32, n,  def 1,     0..10 */
#define ESPLINK_PARAM_UI_OV_BEEP_GAP_MS       43u  /* u32, ms, def 0,     0..5000, >=100 when 42>1 */
#define ESPLINK_PARAM_UI_BL_LED_PERIOD_MS     44u  /* u32, ms, def 1000,  100..10000 */
#define ESPLINK_PARAM_UI_BL_LED_DUTY_PCT      45u  /* u32, %,  def 50,    0..100 */
#define ESPLINK_PARAM_UI_BL_BEEP_PERIOD_MS    46u  /* u32, ms, def 3000,  0=off else 1000..600000 */
#define ESPLINK_PARAM_UI_BL_BEEP_DUR_MS       47u  /* u32, ms, def 233,   per beep (legacy 900 window), 0..fit */
#define ESPLINK_PARAM_UI_BL_BEEP_COUNT        48u  /* u32, n,  def 3,     0..10 */
#define ESPLINK_PARAM_UI_BL_BEEP_GAP_MS       49u  /* u32, ms, def 100,   0..5000, >=100 when 48>1 */
#define ESPLINK_PARAM_UI_RUN_BEEP_START_PCT   50u  /* u32, %,  def 40,    0..100, >= 51 */
#define ESPLINK_PARAM_UI_RUN_BEEP_DOUBLE_PCT  51u  /* u32, %,  def 20,    0..100, <= 50, >= 52 */
#define ESPLINK_PARAM_UI_RUN_BEEP_TRIPLE_PCT  52u  /* u32, %,  def 10,    0..100, <= 51, >= 53 */
#define ESPLINK_PARAM_UI_RUN_BEEP_CRIT_PCT    53u  /* u32, %,  def 1,     0..100, <= 52 */
#define ESPLINK_PARAM_UI_RUN_STD_INTERVAL_MS  54u  /* u32, ms, def 60000, 0=off else 1000..600000 */
#define ESPLINK_PARAM_UI_RUN_TRI_INTERVAL_MS  55u  /* u32, ms, def 20000, 0=off else 1000..600000 */
#define ESPLINK_PARAM_UI_RUN_CRIT_PERIOD_MS   56u  /* u32, ms, def 10000, 0=off else 1000..600000 */
#define ESPLINK_PARAM_UI_RUN_CRIT_DUTY_PCT    57u  /* u32, %,  def 100,   0..100 */
#define ESPLINK_PARAM_UI_RUN_CRIT_COUNT       58u  /* u32, n,  def 1,     0..10 */
#define ESPLINK_PARAM_UI_RUN_STD_DUR_MS       59u  /* u32, ms, def 1000,  per beep, 0..fit */
#define ESPLINK_PARAM_UI_RUN_TRI_DUR_MS       60u  /* u32, ms, def 2000,  per beep, 0..fit */
#define ESPLINK_PARAM_UI_RUN_CRIT_DUR_MS      61u  /* u32, ms, def 10000, 0..120000 one-shot latch */
#define ESPLINK_PARAM_UI_RUN_STD_COUNT        62u  /* u32, n,  def 1,     0..10 */
#define ESPLINK_PARAM_UI_RUN_DOUBLE_COUNT     63u  /* u32, n,  def 2,     0..10 */
#define ESPLINK_PARAM_UI_RUN_TRI_COUNT        64u  /* u32, n,  def 3,     0..10 */
#define ESPLINK_PARAM_UI_RUN_GAP_MS           65u  /* u32, ms, def 100,   0..5000, >=100 when any band count>1 */
#define ESPLINK_PARAM_UI_GREEN_PERIOD_MS      66u  /* u32, ms, def 1000,  100..10000 */
#define ESPLINK_PARAM_UI_GREEN_MIN_OFF_MS     67u  /* u32, ms, def 10,    0..10000, <= 66 */
#define ESPLINK_PARAM_UI_YELLOW_PERIOD_MS     68u  /* u32, ms, def 1000,  100..10000 */
#define ESPLINK_PARAM_UI_YELLOW_MIN_ON_MS    69u  /* u32, ms, def 150,   0..10000, <= 68 (v1.17: 10->150, visible end-of-charge blink) */
#define ESPLINK_PARAM_UI_OV_THRESH_MV         70u  /* u32, mV, def 28000, 24000..32000 */
#define ESPLINK_PARAM_UI_OV_HYST_MV           71u  /* u32, mV, def 1000,  0..2000 */
#define ESPLINK_PARAM_UI_LOWBAT_THRESH_MV     72u  /* u32, mV, def 21000, 15000..24000, <= 73 */
#define ESPLINK_PARAM_UI_LOWBAT_CLEAR_MV      73u  /* u32, mV, def 21200, 15000..24000, >= 72 */
#define ESPLINK_PARAM_UI_PCT_VMIN_MV          74u  /* u32, mV, def 21000, 15000..25000, <= 75-100 - DISCHARGE map (v1.49) */
#define ESPLINK_PARAM_UI_PCT_VMAX_MV          75u  /* u32, mV, def 29000, 25000..32000, >= 74+100 - DISCHARGE map (v1.49) */
#define ESPLINK_PARAM_UI_BUZZER_MUTE          76u  /* u32, 0/1, def 0,    panel-session only (RAM); scenarios only */
#define ESPLINK_PARAM_UI_CHG_FULL_ENTER_PCT  77u  /* u32, %,  def 100,   1..100, enter authoritative */
#define ESPLINK_PARAM_UI_CHG_FULL_EXIT_PCT   78u  /* u32, %,  def 95,    0..100, < 77 after clamp */
#define ESPLINK_PARAM_UI_CHG_HYST_PCT        79u  /* u32, %,  def 5,     0..50 */
#define ESPLINK_PARAM_UI_RUN_HYST_PCT        80u  /* u32, %,  def 2,     0..50 */
#define ESPLINK_PARAM_UI_RUN_ZERO_EXIT       81u  /* u32, %,  def 2,     0..100 */
#define ESPLINK_PARAM_UI_RUN_ONE_EXIT        82u  /* u32, %,  def 3,     0..100 */
/* [EN] Two-loop CC/CV PID charge regulator, ids 83..92 (v1.22, user order
 *      2026-09-28). Dense and in the same order as charger_pid_t packs
 *      them: enable, then one complete (Kp, Ki, Kd, up-rate, down-rate)
 *      row per stage. What a "stage" is: stage 1 is the CURRENT (bulk)
 *      loop, stage 2 is the VOLTAGE loop while the pack is below the
 *      absorb setpoint, stage 3 is the VOLTAGE loop on it and above.
 *      Units: Kp is permille of duty per volt (voltage rows) or per amp
 *      (current row); Ki is milli-permille per second per millivolt or
 *      milliamp of error, i.e. Ki = 1000 means 1 permille/s per volt; slew
 *      rates are milli-permille per second (1000 = 1 permille/s). See the
 *      regulator block in charger.h for the full derivation.
 * [FA] تنظیم‌کنندهٔ PID دوحلقه‌ای CC/CV شارژ، شناسه‌های ۸۳..۹۲ (v1.22، دستور
 *      کاربر ۲۰۲۶-۰۹-۲۸). پشت‌سرهم و دقیقاً به ترتیب فیلدهای charger_pid_t:
 *      فعال‌سازی، سپس برای هر مرحله یک ردیف کامل (Kp، Ki، Kd، نرخ صعود،
 *      نرخ نزول). «مرحله» یعنی: مرحلهٔ ۱ حلقهٔ جریان (بالک)، مرحلهٔ ۲ حلقهٔ
 *      ولتاژ وقتی پک زیر ست‌پوینت ابزورب است، مرحلهٔ ۳ حلقهٔ ولتاژ روی
 *      ست‌پوینت و بالاتر. واحدها: Kp یعنی پرمیل دیوتی بر ولت (ردیف‌های
 *      ولتاژ) یا بر آمپر (ردیف جریان)؛ Ki یعنی میلی‌پرمیل بر ثانیه به ازای
 *      هر میلی‌ولت یا میلی‌آمپر خطا، پس Ki=۱۰۰۰ یعنی ۱ پرمیل بر ثانیه به
 *      ازای هر ولت؛ شیب‌ها میلی‌پرمیل بر ثانیه (۱۰۰۰ = ۱ پرمیل بر ثانیه).
 *      استدلال کامل در بلوک تنظیم‌کنندهٔ charger.h. */
#define ESPLINK_PARAM_CHG_PID_CURRENT_KP        83u  /* u32, -,    def 12,    0..20000 */
#define ESPLINK_PARAM_CHG_PID_CURRENT_KI        84u  /* u32, -,    def 1600,  0..20000 */
#define ESPLINK_PARAM_CHG_PID_CURRENT_KD        85u  /* u32, -,    def 0,     0..20000 */
#define ESPLINK_PARAM_CHG_PID_CURRENT_UP_RATE   86u  /* u32, m‰/s, def 1000,  10..20000 */
#define ESPLINK_PARAM_CHG_PID_CURRENT_DOWN_RATE 87u  /* u32, m‰/s, def 1000,  10..20000 */
#define ESPLINK_PARAM_CHG_PID_VOLTAGE_KP        88u  /* u32, -,    def 50,    0..20000 */
#define ESPLINK_PARAM_CHG_PID_VOLTAGE_KI        89u  /* u32, -,    def 18000, 0..20000 */
#define ESPLINK_PARAM_CHG_PID_VOLTAGE_KD        90u  /* u32, -,    def 0,     0..20000 */
#define ESPLINK_PARAM_CHG_PID_VOLTAGE_UP_RATE   91u  /* u32, m‰/s, def 10,    10..20000 */
#define ESPLINK_PARAM_CHG_PID_VOLTAGE_DOWN_RATE 92u  /* u32, m‰/s, def 1000,  10..20000 */

/* [EN] Charger limits, backstop gains and stage timers, ids 93..107 (v1.28,
 *      USER-ORDERED 2026-10-03: "put all the gains and the limits and the
 *      parameters in so I can change them in the panel"). These were
 *      compile-time constants; the panel's own PID help even stated that the
 *      two hard backstops "are not adjustable from the panel" and described
 *      the absorb ceiling as a fixed hour. Clamp windows live in ONE table
 *      in charger.c (CHG_LIMIT_ROWS); this header only owns the wire ids.
 * [FA] حدها، گین‌های پشتیبان و تایمرهای مرحله‌ای شارژر، شناسه‌های ۹۳..۱۰۷
 *      (دستور کاربر: «همهٔ گین‌ها و حدها و پارامترها را بگذار تا از پنل
 *      تغییر بدهم»). این‌ها ثابت کامپایل بودند؛ راهنمای PID خود پنل هم
 *      نوشته بود دو پشتیبان سخت «از پنل تنظیم نمی‌شوند» و سقف ابزورب را یک
 *      ساعت ثابت معرفی کرده بود. پنجره‌های گیره در «یک» جدول در charger.c
 *      هستند؛ این هدر فقط صاحب شناسه‌های سیمی است. */
#define ESPLINK_PARAM_CHG_ABSORB_MAX_MS         93u  /* u32, ms, def 3600000, 0..21600000 (0 = no ceiling) */
#define ESPLINK_PARAM_CHG_ABSORB_MAX_ARM_MA     94u  /* u32, mA, def 100,     10..1500 */
#define ESPLINK_PARAM_CHG_ABSORB_HOLD_MS        95u  /* u32, ms, def 600000,  0..7200000 */
#define ESPLINK_PARAM_CHG_TAPER_SUSTAIN_MS      96u  /* u32, ms, def 60000,   1000..600000 */
#define ESPLINK_PARAM_CHG_PID_MAX_STEP_PM       97u  /* u32, ‰,  def 8,       1..100 */
#define ESPLINK_PARAM_CHG_PID_OUT_HYST_MILLI    98u  /* u32, m‰, def 700,     0..999 */
#define ESPLINK_PARAM_CHG_PID_VOLT_FILTER_N     99u  /* u32, -,  def 32,      1..64 (1 = off) */
#define ESPLINK_PARAM_CHG_BACKSTOP_MV          100u  /* u32, mV, def 14800,   13000..14800, down only */
#define ESPLINK_PARAM_CHG_BACKSTOP_GAIN_I      101u  /* u32, -,  def 100,     0..2000 */
#define ESPLINK_PARAM_CHG_BACKSTOP_GAIN_V      102u  /* u32, -,  def 500,     0..2000 */
#define ESPLINK_PARAM_CHG_PID_CUR_MARGIN_MA    103u  /* u32, mA, def 10,      0..100 */
#define ESPLINK_PARAM_CHG_CONNECT_SETTLE_MS    104u  /* u32, ms, def 15000,   0..120000 */
#define ESPLINK_PARAM_CHG_JIT_LOCKOUT_MS       105u  /* u32, ms, def 3000,    0..60000 */
#define ESPLINK_PARAM_CHG_MANUAL_WATCHDOG_MS   106u  /* u32, ms, def 3000,    500..60000 */
#define ESPLINK_PARAM_CHG_RAMP_DOWN_INT_MS     107u  /* u32, ms, def 500,     50..5000 */

/* [EN] v1.43 - battery-imbalance scenario 6 (Imbalance): rest/discharge
 *      absolute-delta thresholds, the two charge-window gates, episode
 *      stability + hysteresis, event budget to latch, latch beep cadence,
 *      the output-block checkbox and the charge-cycle budget. The matching
 *      persisted runtime slots (events / latched cycles / latch) are
 *      ESP_LINK_NVM_SLOT_* ids 200..202 - NOT parameters.
 * [FA] سناریوی ۶ (عدم‌توازن باتری): حدها، گیت‌های پنجره، دم، بودجهٔ رویداد
 *      تا قفل، بوق قفل، تیک مسدودی و بودجهٔ سیکل شارژ. اسلات‌های شمارندهٔ
 *      ماندگار ۲۰۰..۲۰۲ پارامتر نیستند. */
#define ESPLINK_PARAM_IMBAL_REST_LIMIT_MV      108u  /* u32, mV, def 300,    0..2000 */
#define ESPLINK_PARAM_IMBAL_DISCH_LIMIT_MV     109u  /* u32, mV, def 500,    0..2000 */
#define ESPLINK_PARAM_IMBAL_REST_WAIT_MS       110u  /* u32, ms, def 600000, 0..3600000 (0 = rest check off) */
#define ESPLINK_PARAM_IMBAL_CHG_WAIT_MS        111u  /* u32, ms, def 600000, 0..3600000 (0 = during-charge check off) */
#define ESPLINK_PARAM_IMBAL_EVENT_STABLE_MS    112u  /* u32, ms, def 30000,  1000..600000 */
#define ESPLINK_PARAM_IMBAL_EVENT_HYST_MV      113u  /* u32, mV, def 100,    0..1000 */
#define ESPLINK_PARAM_IMBAL_EVENT_MAX          114u  /* u8,  def 10,  1..255: latch after N episodes */
#define ESPLINK_PARAM_IMBAL_BEEP_PERIOD_MS     115u  /* u32, ms, def 3600000, 0..86400000 (0 = silent latch) */
#define ESPLINK_PARAM_IMBAL_BEEP_TIME_MS       116u  /* u32, ms, def 200,    20..2000 */
#define ESPLINK_PARAM_IMBAL_BLOCK_OUTPUT       117u  /* bool, def 1 */
#define ESPLINK_PARAM_IMBAL_CHG_CYCLE_MAX      118u  /* u8,  def 20,  1..255: latched charge cycles until charge halt */

/* ==================== Charge-side percent map / نگاشت درصد سمت شارژ ==================== */
/* [EN] v1.49 (user order 2026-10-05): the charge side gets its own
   voltage-to-percent pair, so the discharge limits 74/75 and the full-charge
   latch 77/78 are no longer two uses of one register. Same factory values as
   74/75, so nothing moves until somebody moves it on purpose.
   [FA] سمت شارژ جفت ولتاژ-به-درصد خودش را دارد تا حد دشارژ ۷۴/۷۵ و قفل
   فول‌شارژ ۷۷/۷۸ دو مصرف یک رجیستر نباشند. پیش‌فرض‌ها همان ۷۴/۷۵. */
#define ESPLINK_PARAM_UI_CHG_PCT_VMIN_MV       119u  /* u32, mV, def 21000, 15000..25000, <= 120-100 */
#define ESPLINK_PARAM_UI_CHG_PCT_VMAX_MV       120u  /* u32, mV, def 29000, 25000..32000, >= 119+100 */

/* ==================== Band 2 own beep shape / شکل بوق مخصوص باند ۲ ==================== */
/* [EN] v1.50 (user order): the 2-beep discharge band no longer borrows band
   1's per-beep duration and the all-band gap.
   [FA] باند دو-بوقِ دشارژ دیگر مدت و گپ را قرض نمی‌گیرد. */
#define ESPLINK_PARAM_UI_RUN_DOUBLE_DUR_MS     121u  /* u32, ms, def 1000, 0..fit vs 54/63/122 */
#define ESPLINK_PARAM_UI_RUN_DOUBLE_GAP_MS     122u  /* u32, ms, def 100,  0..5000 */

#define ESPLINK_PARAM_COUNT               123u  /* [EN] 20..26 = profile (v1.12), 27..37 = alarms (v1.15), 38..76 = UI cadence (v1.16), 77..82 = full/hysteresis (v1.17), 83..92 = two-loop CC/CV PID (v1.24), 93..107 = charger limits & backstop gains (v1.28), 108..118 = imbalance scenario 6 (v1.43), 119..120 = charge-side percent map (v1.49), 121..122 = band-2 own beep shape (v1.50). Runtime slots 200..202 are persisted but NOT parameters: they stay outside this count and the GET_PARAMS bulk on purpose. [FA] پروفایل، آلارم‌ها، اعداد UI، PID دوحلقه‌ای، حدها/گین‌های پشتیبان، سناریوی ۶ و نگاشت درصد سمت شارژ (۱۱۹..۱۲۰) و شکل بوق باند ۲ (۱۲۱..۱۲۲)؛ اسلات‌های ۲۰۰..۲۰۲ پارامتر نیستند */

/* ==================== Telemetry layout / چیدمان تله‌متری ==================== */

/* [EN] TLM_LIVE payload (116 bytes, little-endian):
 *        0  u16 sequence (wraps)
 *        2  u8  flags: b0 snapshot valid, b1 input present, b2 meas data
 *                      valid, b3 charger-1 ESP enable, b4 charger-2 ESP
 *                      enable, b5 manual test mode active (v1.2),
 *                      b6..b7 reserved 0
 *        3  u8  imbalance flags (v1.43): b0 episode in progress,
 *                      b1 latched verdict, b2 output veto engaged,
 *                      b3 charge budget spent (charging held off),
 *                      b4..b7 reserved 0
 *        4  u32 raw1_counts        8 u32 shunt1_uv        12 u32 ma1_unfiltered
 *       16  u32 i1_filtered_ma    20 u32 iest1_ma         24 u32 duty1_permille
 *       28  u32 state1            32 u32 raw2_counts      36 u32 shunt2_uv
 *       40  u32 ma2_unfiltered    44 u32 i2_filtered_ma   48 u32 iest2_ma
 *       52  u32 duty2_permille    56 u32 state2           60 u32 v_in_mv
 *       64  u32 v_bat24_mv        68 u32 v_bat12_mv       72 u32 v_bat_low_mv
 *       76  u32 v_bat_high_mv     80 u32 fault_mask
 *       84  u32 vin_raw_counts    88 u32 v24_raw_counts   92 u32 v12_raw_counts
 *       96  u32 vrefint_counts   100 u32 vdda_mv
 *      104  u32 imbalance_mv     108 u32 imbalance_events 112 u32 latched_cycles
 *      [EN] v1.25: the last five are CALIBRATION GROUND TRUTH. Counts are the
 *      only numbers on this board no coefficient can distort, so logging them
 *      beside a DMM lets every scale be rebuilt from first principles instead
 *      of being tuned on top of whatever the firmware already believes.
 *      [FA] پنج فیلد آخر مبنای کالیبراسیون‌اند: شمارش تنها عددی است که هیچ
 *      ضریبی خرابش نمی‌کند، پس ثبتشان کنار مولتی‌متر اجازه می‌دهد هر مقیاس از
 *      پایه بازساخته شود نه اینکه روی باور فعلی فرم‌ور تنظیم شود.
 *      Channel 1 = Trans1 / upper battery, channel 2 = Trans2 / lower
 *      battery. Charger states: 0 OFF, 1 BULK, 2 ABSORB, 3 FLOAT, 4 BRINGUP,
 *      5 JIT_RETRY_WAIT, 6 INPUT_WAIT, 7 FINAL_FAULT, 8 BAT_LOST,
 *      9 MANUAL (v1.2).
 * [FA] payload ی TLM_LIVE (۱۱۶ بایت از نسخهٔ ۱٫۴۳، اندیان کوچک): ترتیب
 *      فیلدها مثل جدول بالا؛ بایت ۳ از v1.43 فلگ‌های عدم‌توازن است و سه
 *      فیلد انتهایی (۱۰۴/۱۵۸/۱۱۲) بلوک زندهٔ آن. کانال ۱ = Trans1 / باتری
 *      بالا و کانال ۲ = Trans2 / باتری پایین. وضعیت شارژر: 0 OFF تا 8
 *      BAT_LOST. */
#define ESPLINK_TLM_PAYLOAD_SIZE     116u

/* ==================== Functions ==================== */

/**
 * @brief  [EN] Bring the link up: select the UART backend, reset the parser
 *              and power the ESP (CH_PD high). Runs only while MODULE_ESP
 *              is enabled.
 *         [FA] لینک را بالا می‌آورد: انتخاب backend ی UART، ریست پارسر و
 *              روشن‌کردن ESP (CH_PD.high). فقط وقتی MODULE_ESP فعال است
 *              اجرا می‌شود.
 */
void func__EspLink_Init(void);

/**
 * @brief  [EN] Apply one parameter id/value through the clamped setters and
 *              report the applied value. Public since v1.14: the boot-time
 *              flash load (esp_link_nvm.c) replays the persisted record
 *              through this SAME path, so a stored value can only land
 *              inside the compiled safety windows.
 *         [FA] اعمال یک شناسه/مقدار پارامتر از setterهای گیره‌دار با گزارش
 *              مقدار اعمال‌شده. عمومی از v1.14: بارگذاری فلش هنگام بوت
 *              (esp_link_nvm.c) رکورد ذخیره‌شده را از همین مسیر بازپخش
 *              می‌کند تا مقدار ذخیره‌شده فقط داخل پنجره‌های ایمنی کامپایل
 *              بنشیند.
 */
bool func__EspLink_ApplyParam(uint8_t uint8_t__paramId,
                              uint32_t uint32_t__value,
                              uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read the live value of one parameter id. Public since v1.14:
 *              the flash save snapshot (esp_link_nvm.c) uses it.
 *         [FA] خواندن مقدار زندهٔ یک شناسهٔ پارامتر. عمومی از v1.14: عکس
 *              ذخیرهٔ فلش (esp_link_nvm.c) از آن استفاده می‌کند.
 */
bool func__EspLink_GetParam(uint8_t uint8_t__paramId, uint32_t *uint32_t__value);

/**
 * @brief  [EN] Send one telemetry frame and consume every received command
 *              frame (user order 2026-09-22). The telemetry carries no
 *              app_state field - the panel derives the system face from
 *              voltages + fault/flag bits - so no state parameter exists
 *              (the dead one was removed, full-program audit 2026-09-26).
 *         [FA] یک فریم تله‌متری می‌فرستد و همهٔ فریم‌های فرمان دریافتی را
 *              مصرف می‌کند (دستور کاربر ۲۰۲۶-۰۹-۲۲). تله‌متری فیلد
 *              app_state ندارد - پنل چهرهٔ سیستم را از ولتاژها و بیت‌ها
 *              می‌سازد - پس پارامتر state وجود ندارد.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 * @param  fault_mask_t__faults [EN] Fault bits / بیت‌های خطا
 */
void func__EspLink_Run(const measurement_snapshot_t *measurement_snapshot_t__snap,
                       fault_mask_t fault_mask_t__faults);

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 * @param  bool__on [EN] true=on, false=off / روشن/خاموش
 */
void func__EspLink_Power(bool bool__on);

#endif /* ESP_LINK_H */
