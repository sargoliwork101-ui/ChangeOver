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
/* [EN] v1.16 (user order 2026-09-26): the length field is u16
 *      little-endian (len_lo + len_hi) - 77 parameters grow PARAMS_BULK
 *      to 1 + 77 x 5 = 386 payload bytes, past the old u8 ceiling of
 *      255. Frame = AA 55 type len_lo len_hi payload xor; the xor covers
 *      type + both length bytes + payload. Both boards MUST flash
 *      together (a v1.15 parser reads len_hi as payload).
 *      / [FA] از v1.16 (دستور کاربر ۲۰۲۶-۰۹-۲۶): فیلد طول u16 لیتل‌اندین
 *      است - ۷۷ پارامتر PARAMS_BULK را به ۱ + ۷۷ × ۵ = ۳۸۶ بایت
 *      می‌رساند که از سقف u8 قبلی (۲۵۵) گذشته است. فریم = AA 55 نوع
 *      len_lo len_hi و xor روی نوع + هر دو بایت طول + payload. هر دو برد
 *      باید با هم فلش شوند. */
#define ESPLINK_FRAME_HEADER_SIZE     5u   /* SOF0 + SOF1 + type + len_lo + len_hi / بدون payload و xor */
#define ESPLINK_FRAME_CHECKSUM_SIZE   1u
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
#define ESPLINK_MSG_CAL_REFERENCE     0x03u

/* [EN] CAL_REFERENCE (protocol v1.3, user order 2026-09-24): one-shot bench
 *      calibration - "send and receive every calibration number via the
 *      ESP". Payload = [target:u8][ref_mA:u32 LE] where ref_mA is the
 *      multimeter reading the user types on the panel:
 *        target 0 = GAIN ch1   1 = GAIN ch2   2 = ETA ch1   3 = ETA ch2
 *      GAIN: new gain = gain x ref / i_filtered(live), clamped 100..3000 by
 *            the setter; that channel's ETA is reset to 0 because the old
 *            ETA absorbed the old gain - rerun target 2/3 afterwards.
 *            Replies with TWO PARAM_REPORT frames: the new gain (param
 *            2/3) and then the reset ETA (param 9/10 = 0).
 *      ETA:  eta = ref x Vbat x 1000 / (i_filtered x Vin) computed from the
 *            LIVE snapshot (channel battery = v_bat_high for ch1,
 *            v_bat_low for ch2); from then on iest = i_filtered x Vin x
 *            eta / (1000 x Vbat) tracks the input and battery voltages
 *            automatically as the battery charges. Replies with ONE
 *            PARAM_REPORT (param 9/10).
 *      Rejection (NO reply frame at all): snapshot missing/invalid, ref
 *      outside ESPLINK_CAL_MIN_REF_MA..ESPLINK_CAL_MAX_REF_MA, live
 *      filtered current below ESPLINK_CAL_MIN_REF_MA, or (ETA only)
 *      Vin/Vbat below the charger minimums (CHG_ETA_MIN_VIN_MV /
 *      CHG_ETA_MIN_VBAT_MV).
 * [FA] CAL_REFERENCE (پروتکل v1.3، دستور کاربر ۲۰۲۶-۰۹-۲۴): کالیبراسیون
 *      یک‌مرحله‌ای بنچ — «همهٔ اعداد کالیبراسیون از ESP فرستاده/دریافت
 *      شود». payload = [target:u8][ref_mA:u32 LE] که ref_mA همان عدد
 *      مولتی‌متری است که کاربر در پنل وارد می‌کند:
 *        target 0 = گین کانال۱   1 = گین کانال۲   2 = η کانال۱   3 = η کانال۲
 *      گین: گین جدید = گین × ref ÷ جریان فیلترشدهٔ زنده؛ setter بین
 *            ۱۰۰..۳۰۰۰ گیره می‌زند؛ η همان کانال صفر می‌شود چون η قدیمی
 *            خطای گین قدیمی را جذب کرده بود — بعدش دوباره target 2/3
 *            بدهید. پاسخ: دو فریم PARAM_REPORT — گین جدید (پارامتر ۲/۳)
 *            و بعد η صفرشده (پارامتر ۹/۱۰).
 *      η:   η = ref × Vbat × ۱۰۰۰ ÷ (جریان فیلترشده × Vin) از snapshot
 *            زنده (باتری کانال = v_bat_high برای ch1 و v_bat_low برای
 *            ch2)؛ از آن به بعد iest = جریان فیلترشده × Vin × η ÷ (۱۰۰۰ ×
 *            Vbat) خودش تغییر ولتاژ ورودی و باتری را در طول شارژ دنبال
 *            می‌کند. پاسخ: یک PARAM_REPORT (پارامتر ۹/۱۰).
 *      رد (هیچ فریم پاسخی نمی‌آید): snapshot نبودن/نامعتبر بودن، ref
 *      بیرون از ESPLINK_CAL_MIN_REF_MA..ESPLINK_CAL_MAX_REF_MA، جریان
 *      فیلترشدهٔ زنده کمتر از ESPLINK_CAL_MIN_REF_MA، یا (فقط η) ولتاژهای
 *      کمتر از حد شارژر (CHG_ETA_MIN_VIN_MV / CHG_ETA_MIN_VBAT_MV).
 */
#define ESPLINK_CAL_MIN_REF_MA               50u
#define ESPLINK_CAL_MAX_REF_MA             5000u
#define ESPLINK_MSG_TLM_LIVE          0x10u
#define ESPLINK_MSG_PARAM_REPORT      0x11u
#define ESPLINK_MSG_PARAMS_BULK       0x12u

/* ==================== Parameter IDs / شناسهٔ پارامترها ==================== */

/* [EN] SET_PARAM payload = [id:u8][value:u32 LE]. Every value is clamped by
 *      the owning module; PARAM_REPORT returns the APPLIED value. Voltage
 *      offsets are signed (two's complement in the u32 wire field).
 *      Ids 0..14 + 20..75 are flash-persisted (v1.14 NVM, ~1.5 s debounce);
 *      only the transient test modes 15..19 (+76) are RAM-only.
 *      Filters carry ONE size parameter each
 *      (user order 2026-09-22: any median 1..15 since v1.4, average
 *      window 1..300; size 1 = bypass, no separate on/off switch).
 * [FA] payload ی SET_PARAM = [id:u8][value:u32 LE]. هر مقدار در ماژول مالکش
 *      گیره می‌شود و PARAM_REPORT مقدارِ اعمال‌شده را برمی‌گرداند. آفست‌های
 *      ولتاژ علامتدارند (متمم دو در فیلد u32 خط). شناسه‌های ۰..۱۴ و
 *      ۲۰..۷۵ روی فلش می‌مانند (NVM نسخهٔ ۱.۱۴، ~۱٫۵ ثانیه)؛ فقط مودهای
 *      گذرای تست ۱۵..۱۹ (+۷۶) فقط-RAM هستند. هر فیلتر یک پارامتر اندازه
 *      دارد (دستور کاربر ۲۰۲۶-۰۹-۲۲: از نسخهٔ ۱.۴ هر مدین ۱..۱۵،
 *      پنجرهٔ میانگین ۱..۳۰۰؛ اندازهٔ ۱ یعنی عبور مستقیم و کلید جدا وجود
 *      ندارد). */
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
#define ESPLINK_PARAM_CHG_PROFILE_BULK_CURRENT_MAX_MA 25u /* u32, mA, def 650,   100..900 */
#define ESPLINK_PARAM_CHG_PROFILE_TAPER_CURRENT_MA   26u  /* u32, mA, def 50,    10..min(300,imax) */
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
#define ESPLINK_PARAM_CHG_ALARM_HARD_CURRENT_MA      35u  /* u32, mA, def 950,   imax+50..950 (down-only) */
#define ESPLINK_PARAM_CHG_ALARM_OV_CUTOFF_MV         36u  /* u32, mV, def 15000, over+150..15000 (down-only) */
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
#define ESPLINK_PARAM_UI_GREEN_MIN_OFF_MS     67u  /* u32, ms, def 10,    0..66 */
#define ESPLINK_PARAM_UI_YELLOW_PERIOD_MS     68u  /* u32, ms, def 1000,  100..10000 */
#define ESPLINK_PARAM_UI_YELLOW_MIN_ON_MS    69u  /* u32, ms, def 10,    0..68 */
#define ESPLINK_PARAM_UI_OV_THRESH_MV         70u  /* u32, mV, def 28000, 24000..32000 */
#define ESPLINK_PARAM_UI_OV_HYST_MV           71u  /* u32, mV, def 1000,  0..2000 */
#define ESPLINK_PARAM_UI_LOWBAT_THRESH_MV     72u  /* u32, mV, def 21000, 15000..24000, <= 73 */
#define ESPLINK_PARAM_UI_LOWBAT_CLEAR_MV      73u  /* u32, mV, def 21200, 15000..24000, >= 72 */
#define ESPLINK_PARAM_UI_PCT_VMIN_MV          74u  /* u32, mV, def 21000, 15000..25000, <= 75-100 */
#define ESPLINK_PARAM_UI_PCT_VMAX_MV          75u  /* u32, mV, def 29000, 25000..32000, >= 74+100 */
#define ESPLINK_PARAM_UI_BUZZER_MUTE          76u  /* u32, 0/1, def 0,    panel-session only (RAM); scenarios only */
#define ESPLINK_PARAM_COUNT                77u  /* [EN] 20..26 = profile (v1.12), 27..37 = alarms (v1.15), 38..76 = UI cadence (v1.16) / [FA] پروفایل، آلارم‌ها و اعداد UI */

/* ==================== Telemetry layout / چیدمان تله‌متری ==================== */

/* [EN] TLM_LIVE payload (84 bytes, little-endian):
 *        0  u16 sequence (wraps)
 *        2  u8  flags: b0 snapshot valid, b1 input present, b2 meas data
 *                      valid, b3 charger-1 ESP enable, b4 charger-2 ESP
 *                      enable, b5 manual test mode active (v1.2),
 *                      b6..b7 reserved 0
 *        3  u8  reserved 0
 *        4  u32 raw1_counts        8 u32 shunt1_uv        12 u32 ma1_unfiltered
 *       16  u32 i1_filtered_ma    20 u32 iest1_ma         24 u32 duty1_permille
 *       28  u32 state1            32 u32 raw2_counts      36 u32 shunt2_uv
 *       40  u32 ma2_unfiltered    44 u32 i2_filtered_ma   48 u32 iest2_ma
 *       52  u32 duty2_permille    56 u32 state2           60 u32 v_in_mv
 *       64  u32 v_bat24_mv        68 u32 v_bat12_mv       72 u32 v_bat_low_mv
 *       76  u32 v_bat_high_mv     80 u32 fault_mask
 *      Channel 1 = Trans1 / upper battery, channel 2 = Trans2 / lower
 *      battery. Charger states: 0 OFF, 1 BULK, 2 ABSORB, 3 FLOAT, 4 BRINGUP,
 *      5 JIT_RETRY_WAIT, 6 INPUT_WAIT, 7 FINAL_FAULT, 8 BAT_LOST,
 *      9 MANUAL (v1.2).
 * [FA] payload ی TLM_LIVE (۸۴ بایت، اندیان کوچک): ترتیب فیلدها مثل جدول
 *      بالا؛ کانال ۱ = Trans1 / باتری بالا و کانال ۲ = Trans2 / باتری
 *      پایین. وضعیت شارژر: 0 OFF تا 8 BAT_LOST. */
#define ESPLINK_TLM_PAYLOAD_SIZE      84u

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
