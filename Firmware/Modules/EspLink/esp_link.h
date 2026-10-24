/**
 * @file    esp_link.h
 * @brief   [EN] ESP-Link: power control, binary command protocol and
 *              telemetry over the USART1 board port (user order
 *              2026-09-22: the ESP panel monitors and retunes the
 *              measurement/calibration controls and can cut/reconnect each
 *              charger module).
 *          [‎FA] ESP-Link‎: کنترل تغذیه، پروتکل باینری فرمان و تله‌متری روی
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

/* [‎EN] Start-of-frame bytes and geometry. / [FA]‎ بایت‌های شروع فریم و هندسه. */
#define ESPLINK_SOF_BYTE0             0xAAu
#define ESPLINK_SOF_BYTE1             0x55u
/* [EN] Since v2 the length field is u16 little-endian and the frame carries
 *      an explicit version plus CRC-16/CCITT-FALSE. PARAMS_BULK is chunked:
 *      the 512-byte payload ceiling holds at most 102 id/value items per
 *      frame, so the current 143-id parameter space is sent in multiple frames.
 *      Both boards MUST flash together because a v1 parser reads the version
 *      byte as the old payload geometry.
 * [FA] از v2 طول فریم u16 لیتل‌اندین، نسخهٔ صریح و ‎CRC-16/CCITT-FALSE‎ دارد.
 *      PARAMS_BULK تکه‌تکه است: سقف payload برابر ۵۱۲ بایت است و هر فریم حداکثر
 *      ۱۰۲ جفت شناسه/مقدار می‌برد؛ فضای ۱۴۳شناسه‌ای فعلی در چند فریم ارسال می‌شود.
 *      هر دو برد باید با هم فلش شوند چون پارسر قدیمی بایت نسخه را payload می‌خواند. */
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
 *      ۱. ‎CRC-16/CCITT-FALSE‎ جای ‎XOR-8‎ را می‌گیرد. ‎XOR-8‎ تقریباً یک از ۲۵۶ خرابی
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
 * [FA] انواع پیام. ESP به ‎STM: SET_PARAM / GET_PARAMS / CAL_REFERENCE‎
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
/* [EN] v1.66 LUT PUSH (user order 2026-10-05: "do I really have to paste the
 *      table into the code and rebuild? push it straight into the micro - and
 *      keep the current way too"). Four ESP -> STM push messages and staged
 *      ACKs; the active-table readback adds one request and two replies:
 *        0x04 LUT_BEGIN  [n1:u8][n2:u8]                 open a staging buffer
 *        0x05 LUT_CHUNK  [ch:u8][first:u8][count:u8]
 *                        + count x (counts:u32, batteryMa:u32)
 *        0x06 LUT_COMMIT [crc32:u32 of the staged CONTENT]
 *        0x07 LUT_RESET  ['R','S','T','!']              reboot after handshake
 *        0x08 LUT_READ   []                             read the active points
 *        0x13 LUT_ACK    [stage:u8][status:u8][n1:u8][n2:u8][crc32:u32]
 *        0x14 LUT_DATA   [ch:u8][first:u8][count:u8]
 *                       + count x (counts:u32, batteryMa:u32)
 *        0x15 PARAM_NAK  [id:u8][reason:u8]   (v2: a SET_PARAM that was NOT applied; 1 = not applied, 2 = bad length)
 *      ACK n1/n2 echo the point counts the CURRENT BEGIN declared (NOT the
 *      active table), so the sender can match each ACK against its own push.
 *      LUT_DATA is the diagnostic readback of the active RAM/flash table; the
 *      panel compares every returned pair and never treats CRC/count as a
 *      substitute for the values. The table NEVER travels as parameters: it
 *      has its own messages and its own flash block (cal_lut.c), so a push can
 *      neither disturb the parameter record nor be mistaken for one.
 * [FA] ارسال مستقیم جدول (v1.66، دستور کاربر ۲۰۲۶-۱۰-۰۵): چهار پیام push از ESP
 *      به STM و ACKهای مرحله‌ای. در v1.85 یک درخواست خواندن و دو پاسخ مستقل
 *      برای خواندن واقعی نقاط فعال اضافه شده‌اند. ACK تعداد نقاط BEGIN همین
 *      تراکنش را بازمی‌گرداند؛ LUT_DATA نقاط فعال RAM/فلش را عددبه‌عدد به پنل
 *      می‌دهد و پنل مقدارها را مقایسه می‌کند، نه اینکه CRC/تعداد را جایگزین
 *      عددها بداند. جدول هرگز به شکل «پارامتر» سفر نمی‌کند و بلوک فلش جداگانهٔ
 *      خودش را دارد. */
#define ESPLINK_MSG_LUT_BEGIN         0x04u
#define ESPLINK_MSG_LUT_CHUNK         0x05u
#define ESPLINK_MSG_LUT_COMMIT        0x06u
#define ESPLINK_MSG_LUT_RESET         0x07u
#define ESPLINK_MSG_LUT_READ          0x08u
#define ESPLINK_MSG_LUT_ACK           0x13u
#define ESPLINK_MSG_LUT_DATA          0x14u
/* [EN] v2 NAK (user approval 2026-10-12): STM -> ESP, payload [id:u8][reason:u8].
 *      Sent for every SET_PARAM that is NOT applied. Reasons below.
 *      [FA] NAK نسخهٔ ۲ (تأیید کاربر ۲۰۲۶-۱۰-۱۲): برای هر SET_PARAM که اعمال نشد. */
#define ESPLINK_MSG_PARAM_NAK         0x15u
#define ESPLINK_NAK_NOT_APPLIED       0x01u  /* [EN] unknown id or owner refused / [FA] شناسهٔ ناشناخته یا رد مالک */
#define ESPLINK_NAK_BAD_LENGTH        0x02u  /* [EN] payload is not 5 bytes / [FA] طول payload برابر ۵ نیست */
#define ESPLINK_LUT_ACK_STAGE_BEGIN   1u
#define ESPLINK_LUT_ACK_STAGE_CHUNK   2u
#define ESPLINK_LUT_ACK_STAGE_COMMIT  3u
#define ESPLINK_LUT_ACK_STAGE_RESET   4u

#define ESPLINK_MSG_TLM_LIVE          0x10u
#define ESPLINK_MSG_PARAM_REPORT      0x11u
#define ESPLINK_MSG_PARAMS_BULK       0x12u
/* [EN] Simple panel diag/time messages (0x30..0x32) - compatible with
 *      ESP_USER (user panel). The full technical panel does not
 *      use them, but the STM32 must answer them when the simple panel
 *      is connected: TIME_SYNC (ESP->STM 4-byte LE unix sec), DIAG_REQ
 *      (ESP->STM empty, requests full diag), DIAG_DATA (STM->ESP chunked
 *      diag, see diag.h and simple_link.h).
 * [FA] پیام‌های دیاگ/زمان پنل ساده (۰x۳۰..۰x۳۲) - سازگار با پنل ساده کاربر.
 *      پنل فنی از آنها استفاده نمی‌کند، اما STM32 باید هنگام اتصال پنل
 *      ساده به آنها پاسخ دهد. */
#define ESPLINK_MSG_TIME_SYNC         0x30u
#define ESPLINK_MSG_DIAG_REQ          0x31u
#define ESPLINK_MSG_DIAG_DATA         0x32u
/* [EN] Hybrid Wi-Fi power-saving client state report (ESP -> STM, 1-byte
 *      payload: 1 = client connected, 0 = no client connected) and default
 *      timing/battery-cutoff windows on battery:
 *        - Battery cutoff (< 20 % by default): when running on battery and
 *          battery percentage drops below the threshold, ESP is powered down
 *          completely (CH_PD low) and does not turn on at all.
 *        - First 30 minutes (1800000 ms) on battery with no user connected:
 *          ESP stays ON (CH_PD high) at low antenna TX power.
 *        - After 30 minutes: STM32 cycles CH_PD 2 minutes OFF (120000 ms) and
 *          1 minute ON (60000 ms), while ESP keeps low antenna TX power until
 *          a user connects.
 *        - Whenever mains input is present OR a user is connected (with
 *          battery >= cutoff): ESP stays continuously ON at maximum antenna
 *          TX power, and the 30-minute battery idle timer resets.
 * [FA] گزارش وضعیت اتصال کاربر برای سناریوی ترکیبی کاهش مصرف وای‌فای
 *      (از ESP به STM، یک بایت: ۱ = کاربر متصل، ۰ = بدون کاربر) و مقادیر
 *      پیش‌فرض زمانی و حد قطع باتری:
 *        - حد قطع باتری (پیش‌فرض زیر ۲۰٪): روی باتری اگر درصد باتری زیر این
 *          حد بیاید، ESP کاملاً خاموش می‌شود و دیگر اصلاً روشن نمی‌شود.
 *        - ۳۰ دقیقهٔ اول (۱۸۰۰۰۰۰ میلی‌ثانیه): ESP روشن با قدرت آنتن پایین.
 *        - پس از ۳۰ دقیقه: چرخهٔ ۲ دقیقه خاموش (۱۲۰۰۰۰ میلی‌ثانیه) و ۱ دقیقه
 *          روشن (۶۰۰۰۰ میلی‌ثانیه) با قدرت آنتن پایین تا زمان اتصال کاربر.
 *        - در حضور برق ورودی یا اتصال کاربر (با باتری بالای حد قطع): روشنِ
 *          دائم با بیشترین قدرت آنتن و ریست تایمر ۳۰ دقیقه‌ای. */
#define ESPLINK_MSG_WIFI_CLIENT_STATE       0x33u
#define ESPLINK_WIFI_IDLE_LOW_POWER_MS      1800000u
#define ESPLINK_WIFI_IDLE_LOW_POWER_MIN_MS        0u
#define ESPLINK_WIFI_IDLE_LOW_POWER_MAX_MS 86400000u
#define ESPLINK_WIFI_CYCLE_ON_MS              60000u
#define ESPLINK_WIFI_CYCLE_ON_MIN_MS           5000u
#define ESPLINK_WIFI_CYCLE_ON_MAX_MS        3600000u
#define ESPLINK_WIFI_CYCLE_OFF_MS            120000u
#define ESPLINK_WIFI_CYCLE_OFF_MIN_MS             0u
#define ESPLINK_WIFI_CYCLE_OFF_MAX_MS       3600000u
#define ESPLINK_WIFI_CYCLE_PERIOD_MS        180000u
#define ESPLINK_WIFI_BAT_CUTOFF_PCT              20u
#define ESPLINK_WIFI_BAT_CUTOFF_MIN_PCT           0u
#define ESPLINK_WIFI_BAT_CUTOFF_MAX_PCT         100u
#define ESPLINK_WIFI_CLIENT_HOLD_MS           15000u

/* ==================== Parameter IDs / شناسهٔ پارامترها ==================== */

/* [EN] SET_PARAM payload = [id:u8][value:u32 LE]. Every value is clamped
 *      by the owning module; PARAM_REPORT returns the APPLIED value.
 *      Voltage offsets are signed (two's complement in the u32 wire
 *      field). Ids 0..14 + 20..75 are flash-persisted (~1.5 s debounce);
 *      only the transient test modes 15..19 (+76) are RAM-only. Filters
 *      carry ONE size parameter each (any median 1..15, average window
 *      1..300; size 1 = bypass, no separate on/off switch).
 * [FA] payload ی ‎SET_PARAM = [id:u8][value:u32 LE]‎؛ هر مقدار در ماژول
 *      مالکش گیره می‌شود و PARAM_REPORT مقدارِ اعمال‌شده را برمی‌گرداند.
 *      آفست‌های ولتاژ علامتدارند (متمم دو در فیلد u32). شناسه‌های ۰..۱۴ و
 *      ۲۰..۷۵ روی فلش می‌مانند؛ فقط مودهای گذرا ۱۵..۱۹ (+۷۶) فقط-RAM
 *      هستند. هر فیلتر یک پارامتر اندازه دارد (مدین ۱..۱۵، میانگین
 *      ۱..۳۰۰؛ ۱ = عبور مستقیم). */
#define ESPLINK_PARAM_CUR1_OFFSET_COUNTS   0u   /* u32, counts,   def 0,    0..255    */
#define ESPLINK_PARAM_CUR2_OFFSET_COUNTS   1u   /* u32, counts,   def 0,    0..255    */
#define ESPLINK_PARAM_CUR1_GAIN_PERMILLE   2u   /* u32, permille, def 3000, 100..3000 */
#define ESPLINK_PARAM_CUR2_GAIN_PERMILLE   3u   /* u32, permille, def 3000, 100..3000 */
#define ESPLINK_PARAM_VIN_OFFSET_MV        4u   /* i32, mV,       def 0,    -5000..5000 (v1.10) */
#define ESPLINK_PARAM_V24_OFFSET_MV        5u   /* i32, mV,       def 0,    -5000..5000 (v1.10) */
#define ESPLINK_PARAM_V12_OFFSET_MV        6u   /* i32, mV,       def 0,    -5000..5000 (v1.10) */
#define ESPLINK_PARAM_FILTER_MEDIAN_SIZE   7u   /* u32, samples,  def 3,    1..15 any, 1..2=bypass (v1.4) */
#define ESPLINK_PARAM_FILTER_AVERAGE_WINDOW 8u  /* u32, samples,  def 10,   1..300, 1=bypass (v1.4, u16 since audit 2026-09-26) */
#define ESPLINK_PARAM_CHG_ETA1_PERMILLE    9u   /* RESERVED: removed with the Vin/Vbat formula; get/set rejected */
#define ESPLINK_PARAM_CHG_ETA2_PERMILLE   10u   /* RESERVED: removed with the Vin/Vbat formula; get/set rejected */
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
 *      ESP_AGENT_SPEC.md section 19 for the full contract (hardware
 *      ‎floor, JIT re-arm, 3 s link dead-man). / [FA]‎ مود تست دستی سراسری
 *      v1.2 (دستور کاربر): ۱ = شارژر خودکار تعلیق، شرط‌های باتری رد و
 *      درایو مستقیم هر کانال با پارامتر ۱۶/۱۸؛ قرارداد کامل در
 *      ESP_AGENT_SPEC.md بخش 19 (کف سخت‌افزاری، ‎re-arm‎ ی JIT، ددمن ۳
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
#define ESPLINK_PARAM_FAULT_ALARM_DISCONNECT_MV      27u  /* u32, mV, def 14800, 14000..15000 independent */
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
#define ESPLINK_PARAM_UI_RUN_CRIT_BEEP_DUR_MS 57u  /* u32, ms, def 10000, per beep, 0..600000 (v1.71) */
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
/* [EN] v1.74 (user order): ids 72/73 were the low-battery alarm window. The
   window moved into Firmware/Modules/Changeover as fixed constants, so these
   two ids are RETIRED: no owner accepts them, they are never persisted and the
   panel has no field for them. They are kept as holes on purpose - renumbering
   the ids above them would silently change the meaning of every stored NVM
   record, backup file and bench log.
   [FA] شناسه‌های ۷۲/۷۳ بازنشسته‌اند: پنجرهٔ باتری کم به‌صورت ثابت داخل ماژول
   چنج‌اور رفت. عمداً به‌صورت «حفره» می‌مانند تا شماره‌های بالاتر جابه‌جا نشوند. */
#define ESPLINK_PARAM_RETIRED_LOWBAT_FIRST    72u  /* [EN] retired v1.74 / [FA] بازنشسته */
#define ESPLINK_PARAM_RETIRED_LOWBAT_LAST     73u  /* [EN] retired v1.74 / [FA] بازنشسته */
#define ESPLINK_PARAM_UI_PCT_VMIN_MV          74u  /* u32, mV, def 21000, 15000..25000, <= 75-100 - DISCHARGE map (v1.49) */
#define ESPLINK_PARAM_UI_PCT_VMAX_MV          75u  /* u32, mV, def 29000, 25000..32000, >= 74+100 - DISCHARGE map (v1.49) */
#define ESPLINK_PARAM_UI_BUZZER_MUTE          76u  /* u32, 0/1, def 0,    panel-session only (RAM); scenarios only */
#define ESPLINK_PARAM_UI_CHG_FULL_ENTER_PCT  77u  /* u32, %,  def 100,   1..100, enter authoritative */
#define ESPLINK_PARAM_UI_CHG_FULL_EXIT_PCT   78u  /* u32, %,  def 95,    0..100, < 77 after clamp */
#define ESPLINK_PARAM_UI_CHG_HYST_PCT        79u  /* u32, %,  def 5,     0..50 */
#define ESPLINK_PARAM_UI_RUN_HYST_PCT        80u  /* u32, %,  def 2,     0..50 */
#define ESPLINK_PARAM_UI_RUN_ZERO_EXIT       81u  /* u32, %,  def 2,     1..100 (v1.92: 0 made the 0% test always true) */
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
 * [FA] تنظیم‌کنندهٔ PID دوحلقه‌ای ‎CC/CV‎ شارژ، شناسه‌های ۸۳..۹۲ (v1.22، دستور
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

/* [EN] v1.43 - battery-imbalance scenario 5 (Imbalance): rest/discharge
 *      absolute-delta thresholds, the two charge-window gates, episode
 *      stability + hysteresis, event budget to latch, latch beep cadence,
 *      the output-block checkbox and the charge-cycle budget. The matching
 *      persisted runtime slots (events / latched cycles / latch) are
 *      ESP_LINK_NVM_SLOT_* ids 200..202 - NOT parameters.
 * [FA] سناریوی ۵ (عدم‌توازن باتری): حدها، گیت‌های پنجره، دم، بودجهٔ رویداد
 *      تا قفل، بوق قفل، تیک مسدودی و بودجهٔ سیکل شارژ. اسلات‌های شمارندهٔ
 *      ماندگار ۲۰۰..۲۰۲ پارامتر نیستند. */
#define ESPLINK_PARAM_IMBAL_REST_LIMIT_MV      108u  /* u32, mV, def 300,    0..2000 */
#define ESPLINK_PARAM_IMBAL_DISCH_LIMIT_MV     109u  /* u32, mV, def 500,    0..2000 */
#define ESPLINK_PARAM_IMBAL_REST_WAIT_MS       110u  /* u32, ms, def 600000, 0..36000000 (0 = rest check off; max 10 h) */
#define ESPLINK_PARAM_IMBAL_CHG_WAIT_MS        111u  /* u32, ms, def 600000, 0..36000000 (0 = during-charge check off; max 10 h) */
#define ESPLINK_PARAM_IMBAL_EVENT_STABLE_MS    112u  /* u32, ms, def 30000,  1000..36000000 */
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
/* [EN] v1.50 gave the 2-beep discharge band its own per-beep duration and
   its own gap. v1.71 (user order: "in discharge only the GAP is shared,
   everything else is per band") keeps the duration, turns the second id into
   that band's own repeat interval, and puts every band back on the single
   shared gap, id 65.
   [FA] نسخهٔ ۱٫۵۰ مدت و گپ مخصوص باند دو-بوق را ساخت. نسخهٔ ۱٫۷۱ به دستور
   کاربر فقط گپ را مشترک نگه داشت (شناسهٔ ۶۵) و شناسهٔ دوم را به فاصلهٔ تکرار
   همان باند تبدیل کرد. */
#define ESPLINK_PARAM_UI_RUN_DOUBLE_DUR_MS     121u  /* u32, ms, def 1000,  per beep, 0..fit vs 122/63/65 */
#define ESPLINK_PARAM_UI_RUN_DOUBLE_INTERVAL_MS 122u /* u32, ms, def 60000, 0=off else 1000..600000 (v1.71) */

#define ESPLINK_PARAM_IMBAL_BLINK_PERIOD_MS    123u  /* u32, ms, def 1000, 0 = solid red, else 100..10000 (v1.68) */
#define ESPLINK_PARAM_IMBAL_BLINK_DUTY_PCT     124u  /* u8,  %,  def 50,   5..95 (v1.68) */
/* [EN] v1.72 scenario 6 (dead battery), user order: a battery must never sit
   under charge for ever - after ~24 h of continuous charging without reaching
   Float the pack is condemned (solid red, charging stopped until it is
   replaced). The charger module owns the registers; these are the link ids.
   [FA] سناریوی ۶ (باتری خراب): پس از ۲۴ ساعت شارژ پیوسته و بی‌نتیجه، حکم
   خرابی - قرمز ثابت و قطع شارژ تا تعویض باتری. */
#define ESPLINK_PARAM_DEAD_TIMEOUT_MS          125u  /* u32, ms, def 86400000 (24 h), 0 = off, max 172800000 */
#define ESPLINK_PARAM_DEAD_RESET_GAP_MS        126u  /* u32, ms, def 60000 (1 min), pause longer than this zeroes the clock */
#define ESPLINK_PARAM_DEAD_BLOCK_OUTPUT        127u  /* u8,  0/1, def 0, also keep the condemned pack off the output */

/* [EN] v1.80 (user question: "scenario 6 has a lamp and a beep - why is there
   no box for them in the panel?"): the dead-battery face used to borrow the
   imbalance latch beep and a hard-coded solid red. It owns both now, with
   defaults that reproduce the old behaviour exactly.
   [FA] چهرهٔ سناریوی ۶ دیگر قرضی نیست؛ چهار عدد خودش را دارد. */
#define ESPLINK_PARAM_DEAD_BEEP_PERIOD_MS      128u  /* u32, ms, def 600000, 0 = silent */
#define ESPLINK_PARAM_DEAD_BEEP_LEN_MS         129u  /* u32, ms, def 120, 20..2000 */
#define ESPLINK_PARAM_DEAD_BLINK_PERIOD_MS     130u  /* u32, ms, def 0 = SOLID red, else up to 10000 */
#define ESPLINK_PARAM_DEAD_BLINK_DUTY_PCT      131u  /* u8,  %,  def 50, 5..95 */
#define ESPLINK_PARAM_DEAD_BEEP_COUNT         134u  /* u8, count, def 1, 1..10 */
#define ESPLINK_PARAM_DEAD_BEEP_GAP_MS        135u  /* u32, ms, def 0, 0..5000 */

/* [EN] v1.81: the latched imbalance alarm owns these independent pattern
   controls; defaults 1 beep and 0 ms gap preserve the old sound.
   [FA] نسخهٔ ۱٫۸۱: هشدار قفل عدم‌توازن این کنترل‌های مستقل الگو را دارد؛
   پیش‌فرض یک بوق و گپ صفر صدای قبلی را حفظ می‌کند. */
#define ESPLINK_PARAM_IMBAL_BEEP_COUNT         132u  /* u8, count, def 1, 1..10 */
#define ESPLINK_PARAM_IMBAL_BEEP_GAP_MS        133u  /* u32, ms, def 0, 0..5000 */
#define ESPLINK_PARAM_IMBAL_CLEAN_FULL_CYCLES  136u  /* u8, count, def 3, 1..255 */

/* ==================== Scenario 7: technical board fault ==================== */
/* [EN] Six independent controls: the four buzzer fields do not reuse the
 *      scenario-5/6 count and gap ids, and the last two shape the one shared
 *      phase of all three LEDs. [FA] شش کنترل مستقل سناریوی ۷؛ تعداد و گپ
 *      بوق از سناریوهای ۵ و ۶ جداست و دو عدد آخر فاز مشترک سه LED را می‌سازند. */
#define ESPLINK_PARAM_TECH_BEEP_PERIOD_MS  137u  /* u32, ms, def 3000, 0 = silent */
#define ESPLINK_PARAM_TECH_BEEP_LEN_MS     138u  /* u32, ms, def 200, 0..600000 */
#define ESPLINK_PARAM_TECH_BEEP_COUNT      139u  /* u8, count, def 3, 0..10 */
#define ESPLINK_PARAM_TECH_BEEP_GAP_MS     140u  /* u32, ms, def 100, 0..5000 */
#define ESPLINK_PARAM_TECH_LED_PERIOD_MS   141u  /* u32, ms, def 1000, 100..10000 */
#define ESPLINK_PARAM_TECH_LED_DUTY_PCT    142u  /* u8, %, def 50, 0..100 */

/* [EN] Configurable Wi-Fi battery power-saving parameters (ids 143..146):
 *        143: initial low-TX-power ON duration on battery (ms, def 1800000)
 *        144: cycle ON duration after initial window (ms, def 60000)
 *        145: cycle OFF duration after initial window (ms, def 120000)
 *        146: battery percentage cutoff below which ESP stays completely OFF
 *             on battery power (%, def 20).
 * [FA] پارامترهای قابل تنظیم کاهش مصرف وای‌فای روی باتری (شناسه‌های ۱۴۳..۱۴۶):
 *        ۱۴۳: زمان روشن ماندن اولیه با قدرت آنتن پایین (پیش‌فرض ۱۸۰۰۰۰۰)
 *        ۱۴۴: زمان روشن بودن در چرخهٔ متناوب (پیش‌فرض ۶۰۰۰۰)
 *        ۱۴۵: زمان خاموش بودن در چرخهٔ متناوب (پیش‌فرض ۱۲۰۰۰۰)
 *        ۱۴۶: حد درصد باتری که زیر آن وای‌فای کاملاً خاموش می‌ماند (پیش‌فرض ۲۰٪). */
#define ESPLINK_PARAM_WIFI_IDLE_LOW_POWER_MS  143u  /* u32, ms, def 1800000, 0..86400000 */
#define ESPLINK_PARAM_WIFI_CYCLE_ON_MS        144u  /* u32, ms, def 60000, 5000..3600000 */
#define ESPLINK_PARAM_WIFI_CYCLE_OFF_MS       145u  /* u32, ms, def 120000, 0..3600000 */
#define ESPLINK_PARAM_WIFI_BAT_CUTOFF_PCT     146u  /* u8, %, def 20, 0..100 */

/* ==================== Scenario 8: weak battery ==================== */
/* [EN] The Changeover module owns the scenario-8 verdict and its own eleven
 *      controls; nothing is shared with scenarios 5, 6 or 7.
 * [FA] ماژول Changeover صاحب حکم سناریوی ۸ و یازده کنترل مستقل خودش است؛
 *      هیچ‌چیز با سناریوهای ۵، ۶ و ۷ مشترک نیست. */
#define ESPLINK_PARAM_WEAK_ENABLE             147u  /* u8,  0/1, def 1, scenario-8 verdict on */
#define ESPLINK_PARAM_WEAK_REST_MS            148u  /* u32, ms, def 600000 (10 min), 60000..3600000 */
#define ESPLINK_PARAM_WEAK_CYCLE_GUARD_MS     149u  /* u32, ms, def 60000, 1000..600000 */
#define ESPLINK_PARAM_WEAK_HEALTHY_HOLD_MS    150u  /* u32, ms, def 10000, 1000..300000 */
#define ESPLINK_PARAM_WEAK_LED_PERIOD_MS      151u  /* u32, ms, def 500, 0 = solid red else 100..10000 */
#define ESPLINK_PARAM_WEAK_LED_DUTY_PCT       152u  /* u8,  %,  def 50, 5..95 */
#define ESPLINK_PARAM_WEAK_BEEP_PERIOD_MS     153u  /* u32, ms, def 3000, 0 = silent else 1000..600000 */
#define ESPLINK_PARAM_WEAK_BEEP_LEN_MS        154u  /* u32, ms, def 200, 20..2000 */
#define ESPLINK_PARAM_WEAK_BEEP_COUNT         155u  /* u8,  count, def 3, 0..10 */
#define ESPLINK_PARAM_WEAK_BEEP_GAP_MS        156u  /* u32, ms, def 100, 0..5000 */
#define ESPLINK_PARAM_WEAK_BLOCK_OUTPUT       157u  /* u8,  0/1, def 1, keep the weak pack off the output */
#define ESPLINK_PARAM_WEAK_REST_FLOOR_MV      165u  /* u32, mV, def 25000, 21200..29000, rest voltage that makes a re-cut count as weakness */

/* [EN] ids 0..157 are ordinary parameters; 132/133 shape the imbalance
   beep, 134/135 shape the dead-battery beep, 136 controls the number of
   clean FLOAT-qualified cycles before the imbalance event counter is cleared,
   137..142 configure scenario 7, and 143..146 configure Wi-Fi power saving.
   147..157 configure scenario 8 (weak battery) and belong to Changeover.
   Runtime slots stay outside.
   [FA] شناسه‌های ۰..۱۵۷ پارامتر عادی‌اند؛ ۱۳۲/۱۳۳ شکل بوق عدم‌توازن،
   ۱۳۴/۱۳۵ شکل بوق باتری خراب، ۱۳۶ تعداد سیکل‌های کامل پاک، ۱۳۷..۱۴۲
   سناریوی ۷، ۱۴۳..۱۴۶ کاهش مصرف وای‌فای و ۱۴۷..۱۵۷ سناریوی ۸ (باتری
   ضعیف، متعلق به Changeover) را می‌سازند؛ اسلات‌های زمان‌اجرا بیرون‌اند. */
/* [EN] Scenario LED/beep ticks (v1.87, user order): 158..164 mute the face of
 *      scenario 1..7 on the board; 1 = lamp + buzzer on (default), 0 = silent.
 *      Detection, latching and the output gates are NOT touched - the tick is
 *      an alarm switch, not a scenario disable.
 * [FA] تیک هشدار سناریو (نسخهٔ ۱٫۸۷، دستور کاربر): ۱۵۸..۱۶۴ چهرهٔ سناریوهای
 *      ۱..۷ را روی برد خاموش/روشن می‌کند؛ ۱ = چراغ و بوق روشن (پیش‌فرض) و
 *      ۰ = ساکت. تشخیص و قفل و گیت خروجی دست نمی‌خورند؛ این تیک کلید
 *      هشدار است نه غیرفعال‌کردن سناریو. */
#define ESPLINK_PARAM_SCN1_ALARM               158u  /* u8, 0/1, def 1, scenario 1 LED/beep */
#define ESPLINK_PARAM_SCN2_ALARM               159u  /* u8, 0/1, def 1, scenario 2 LED/beep */
#define ESPLINK_PARAM_SCN3_ALARM               160u  /* u8, 0/1, def 1, scenario 3 LED/beep */
#define ESPLINK_PARAM_SCN4_ALARM               161u  /* u8, 0/1, def 1, scenario 4 LED/beep */
#define ESPLINK_PARAM_SCN5_ALARM               162u  /* u8, 0/1, def 1, scenario 5 LED/beep */
#define ESPLINK_PARAM_SCN6_ALARM               163u  /* u8, 0/1, def 1, scenario 6 LED/beep */
#define ESPLINK_PARAM_SCN7_ALARM               164u  /* u8, 0/1, def 1, scenario 7 LED/beep */

/* [EN] Thermal-rest tuning (v1.102, user order 2026-10-24: "the current we
   go to rest at and the 1 h / 10 min pair must be adjustable"): ids 166..168
   belong to the charger module; zero in any duration disables the feature,
   zero trigger keeps the original automatic rule (bulk band minus 50 mA).
   [FA] تنظیم استراحت حرارتی (نسخهٔ ۱۰۲، دستور کاربر ۲۰۲۶-۱۰-۲۴: «جریانی که
   برای استراحت می‌رویم و آن دو زمان ۱ ساعته/۱۰ دقیقه‌ای باید قابل تنظیم باشند»):
   ۱۶۶..۱۶۸ متعلق به ماژول شارژر استند؛ صفر در هر زمان قابلیت را خاموش می‌کند،
   صفر در آستانه همان قانون خودکار قدیمی را نگه می‌دارد (سقف به‌دوره‌ای پایین منهای ۵۰ میلی‌آمپر). */
#define ESPLINK_PARAM_CHG_THERMAL_TRIGGER_MA     166u  /* u32, mA, def 600, 0..3000 (600 = old rule at factory ceiling 650; 0 = auto: bulk band minus 50) */
#define ESPLINK_PARAM_CHG_THERMAL_CONTINUOUS_MS  167u  /* u32, ms, def 3600000, 0..21600000 (0 = rest off) */
#define ESPLINK_PARAM_CHG_THERMAL_REST_MS        168u  /* u32, ms, def 600000, 0..3600000 (0 = rest off) */

#define ESPLINK_PARAM_COUNT               169u

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
 *                      b4 weak-battery verdict latched (v1.88),
 *                      b5 battery path CUT right now (v1.93),
 *                      b6 thermal rest running (v1.93), b7 reserved 0
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
/* [EN] v1.76 (user order: the dead-battery timer must be counted per
   battery): one more u32 at the end carries charger 2's continuous charge
   time, so 4 + 31 x 4 = 128 B.
   [FA] از v1.76 یک u32 دیگر در انتها زمان شارژ کانال ۲ را می‌برد: ۱۲۸ بایت. */
#define ESPLINK_TLM_PAYLOAD_SIZE     128u

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
 *              the flash save snapshot (esp_link_nvm.c) uses it. A NULL
 *              output pointer is rejected before dispatch.
 *         [FA] خواندن مقدار زندهٔ یک شناسهٔ پارامتر. عمومی از v1.14: عکس
 *              ذخیرهٔ فلش (esp_link_nvm.c) از آن استفاده می‌کند؛ خروجی NULL
 *              پیش از dispatch رد می‌شود.
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
 * @param  bool__on [EN] true=on, false=off‎ / روشن/خاموش
 */
void func__EspLink_Power(bool bool__on);

/**
 * @brief  [EN] Query the LOGICAL power state the policy wants. The physical
 *              pin can still be up for a short grace after this returns false
 *              (see func__EspLink_IsPinPowered).
 *         [FA] بررسی وضعیت منطقی تغذیه که سیاست می‌خواهد. پایهٔ فیزیکی ممکن
 *              است پس از false شدن این، تا مهلتِ کوتاهی هنوز بالا باشد
 *              (به ‎func__EspLink_IsPinPowered‎ نگاه کنید).
 * @‎return [EN] true if the policy keeps the ESP on / [FA]‎ اگر سیاست ESP را
 *              روشن نگه دارد true
 */
bool func__EspLink_IsPowerOn(void);

/**
 * @brief  [EN] Query the PHYSICAL CH_PD state: it stays high for
 *              ESPLINK_POWER_OFF_GRACE_MS after the logical off so the last
 *              telemetry frame reaches the panel.
 *         [FA] وضعیت فیزیکی CH_PD: پس از خاموشی منطقی تا مهلتِ خاموشی بالا
 *              می‌ماند تا آخرین فریم تله‌متری به پنل برسد.
 * @‎return [EN] true if the pin is still up / [FA]‎ اگر پایه هنوز بالاست true
 */
bool func__EspLink_IsPinPowered(void);

/**
 * @brief  [EN] Query whether a Wi-Fi client is currently considered connected.
 *         [FA] بررسی متصل بودن کاربر به وای‌فای در وضعیت فعلی.
 * @return [EN] true if a Wi-Fi client is active, false otherwise /
 *         [FA] در صورت اتصال کاربر true و در غیر این صورت false
 */
bool func__EspLink_IsWifiClientConnected(void);

/**
 * @brief  [EN] Return the elapsed battery idle time (ms) without a connected
 *              Wi-Fi user.
 *         [FA] زمان سپری‌شدهٔ بی‌کاری روی باتری بدون اتصال کاربر (میلی‌ثانیه)
 *              را برمی‌گرداند.
 * @return [EN] Elapsed idle milliseconds on battery /
 *         [FA] زمان سپری‌شده بر حسب میلی‌ثانیه
 */
uint32_t func__EspLink_GetBatteryIdleElapsedMs(void);

/**
 * @brief  [EN] Record the latest Wi-Fi client connection state reported by
 *              the ESP or inferred from an interactive user command.
 *         [FA] ثبت آخرین وضعیت اتصال کاربر به وای‌فای (گزارش‌شده از ESP یا
 *              ناشی از فرمان مستقیم کاربر).
 * @param  bool__connected [EN] true when a user is connected, false when idle /
 *                         [FA] در صورت حضور کاربر true و در صورت قطع کامل false
 */
void func__EspLink_NotifyWifiClientState(bool bool__connected);

/**
 * @brief  [EN] Update the latest battery state-of-charge percentage (0..100)
 *              used by the Wi-Fi battery cutoff policy.
 *         [FA] به‌روزرسانی آخرین درصد شارژ باتری (۰ تا ۱۰۰) برای اعمال حد قطع
 *              وای‌فای در باتری ضعیف.
 * @param  uint8_t__batteryPct [EN] Battery percentage (0..100) /
 *                             [FA] درصد شارژ باتری (۰ تا ۱۰۰)
 */
void func__EspLink_SetBatteryPercent(uint8_t uint8_t__batteryPct);

/**
 * @brief  [EN] Read the battery state-of-charge percentage (0..100) currently
 *              seen by the Wi-Fi power-saving policy.
 *         [FA] خواندن درصد شارژ باتری (۰ تا ۱۰۰) در سیاست کاهش مصرف وای‌فای.
 * @‎return [EN] Battery percentage (0..100) / [FA]‎ درصد شارژ باتری
 */
uint8_t func__EspLink_GetBatteryPercent(void);

/**
 * @brief  [EN] Advance the hybrid ESP Wi-Fi power-saving state machine by
 *              uint32_t__stepMs and drive CH_PD accordingly:
 *              - Mains input present: CH_PD stays ON and the battery idle
 *                timer resets to 0.
 *              - On battery with battery percentage below the configured
 *                cutoff (default 20 %): CH_PD is driven OFF completely and
 *                does not turn on at all.
 *              - On battery at/above cutoff with user connected: CH_PD stays
 *                ON and the battery idle timer resets to 0.
 *              - On battery at/above cutoff with no user connected: CH_PD
 *                stays ON for the configured initial low-power window
 *                (default 30 minutes = 1800000 ms), then cycles OFF/ON
 *                (default 2 minutes OFF / 1 minute ON).
 *         [FA] پیشبرد ماشین حالت کاهش مصرف وای‌فای به اندازهٔ uint32_t__stepMs
 *              و کنترل پایهٔ CH_PD:
 *              - حضور برق ورودی: روشن ماندن دائم و صفر شدن تایمر بی‌کاری باتری.
 *              - روی باتری با درصد زیر حد قطع (پیش‌فرض ۲۰٪): خاموش شدن کامل
 *                ESP و روشن نشدن در هیچ حالتی.
 *              - روی باتری بالای حد قطع با حضور کاربر: روشن ماندن دائم.
 *              - روی باتری بالای حد قطع و بدون کاربر: روشن ماندن در بازهٔ
 *                اولیه (پیش‌فرض ۳۰ دقیقه) و سپس چرخهٔ خاموش/روشن متناوب.
 * @param  bool__inputPresent [EN] true if mains input is present /
 *                            [FA] وضعیت حضور برق ورودی
 * @param  uint32_t__stepMs   [EN] Elapsed step in milliseconds /
 *                            [FA] گام زمانی سپری‌شده بر حسب میلی‌ثانیه
 */
void func__EspLink_PowerSaveTick(bool bool__inputPresent,
                                 uint32_t uint32_t__stepMs);

#ifdef ESPLINK_HOST_TEST
/* [EN] Host-only probes for the production parser and LUT-reset handshake. */
void func__EspLink_HostTest_Reset(void);
void func__EspLink_HostTest_FeedByte(uint8_t uint8_t__byte);
uint32_t func__EspLink_HostTest_CrcErrors(void);
uint32_t func__EspLink_HostTest_VersionErrors(void);
uint32_t func__EspLink_HostTest_AcceptedFrames(void);
void func__EspLink_HostTest_RecordCommitAck(bool bool__success);
bool func__EspLink_HostTest_TryReset(bool bool__magicValid,
                                     bool bool__tableActive);
bool func__EspLink_HostTest_HandleLutFrame(uint8_t uint8_t__messageType,
                                           const uint8_t *uint8_t__payload,
                                           uint16_t uint16_t__payloadLength);
#endif

#endif /* ESP_LINK_H */
