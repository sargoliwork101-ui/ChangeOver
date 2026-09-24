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
#define ESPLINK_FRAME_HEADER_SIZE     4u   /* SOF0 + SOF1 + type + len / بدون payload و xor */
#define ESPLINK_FRAME_CHECKSUM_SIZE   1u
/* [EN] 112 since protocol v1.2 (user order 2026-09-23): the 20th parameter
 *      grew PARAMS_BULK to 1 + 20 x 5 = 101 payload bytes. / [FA] از
 *      پروتکل v1.2 (دستور کاربر): پارامتر بیستم PARAMS_BULK را به
 *      1 + 20 × 5 = ۱۰۱ بایت payload رساند. */
#define ESPLINK_FRAME_MAX_PAYLOAD     112u

/* [EN] Message types. ESP -> STM: SET_PARAM / GET_PARAMS. STM -> ESP:
 *      TLM_LIVE (periodic), PARAM_REPORT (after each SET), PARAMS_BULK
 *      (answer to GET). Unknown types are dropped silently.
 * [FA] انواع پیام. ESP به STM: SET_PARAM / GET_PARAMS. STM به ESP:
 *      TLM_LIVE (دوره‌ای)، PARAM_REPORT (بعد از هر SET)، PARAMS_BULK
 *      (پاسخ GET). نوع ناشناخته بی‌صدا کنار گذاشته می‌شود. */
#define ESPLINK_MSG_SET_PARAM         0x01u
#define ESPLINK_MSG_GET_PARAMS        0x02u
#define ESPLINK_MSG_TLM_LIVE          0x10u
#define ESPLINK_MSG_PARAM_REPORT      0x11u
#define ESPLINK_MSG_PARAMS_BULK       0x12u

/* ==================== Parameter IDs / شناسهٔ پارامترها ==================== */

/* [EN] SET_PARAM payload = [id:u8][value:u32 LE]. Every value is clamped by
 *      the owning module; PARAM_REPORT returns the APPLIED value. Voltage
 *      offsets are signed (two's complement in the u32 wire field).
 *      RAM only - a reboot restores the compiled defaults, the ESP re-applies
 *      its tuned set after boot. Filters carry ONE size parameter each
 *      (user order 2026-09-22: median 1/3/5 and average window 1..10;
 *      size 1 = bypass, there is no separate on/off switch).
 * [FA] payload ی SET_PARAM = [id:u8][value:u32 LE]. هر مقدار در ماژول مالکش
 *      گیره می‌شود و PARAM_REPORT مقدارِ اعمال‌شده را برمی‌گرداند. آفست‌های
 *      ولتاژ علامتدارند (متمم دو در فیلد u32 خط). فقط RAM - ری‌استارت
 *      پیش‌فرض‌های کامپایل را برمی‌گرداند و ESP بعد از بوت مجموعهٔ تنظیم‌شده
 *      خود را دوباره اعمال می‌کند. هر فیلتر یک پارامتر اندازه دارد (دستور
 *      کاربر ۲۰۲۶-۰۹-۲۲: مدین ۱/۳/۵ و پنجرهٔ میانگین ۱..۱۰؛ اندازهٔ ۱ یعنی
 *      عبور مستقیم و کلید جدا وجود ندارد). */
#define ESPLINK_PARAM_CUR1_OFFSET_COUNTS   0u   /* u32, counts,   def 8,    0..255    */
#define ESPLINK_PARAM_CUR2_OFFSET_COUNTS   1u   /* u32, counts,   def 8,    0..255    */
#define ESPLINK_PARAM_CUR1_GAIN_PERMILLE   2u   /* u32, permille, def 1046, 100..3000 */
#define ESPLINK_PARAM_CUR2_GAIN_PERMILLE   3u   /* u32, permille, def 1085, 100..3000 */
#define ESPLINK_PARAM_VIN_OFFSET_MV        4u   /* i32, mV,       def 0,    -2000..2000 */
#define ESPLINK_PARAM_V24_OFFSET_MV        5u   /* i32, mV,       def 0,    -2000..2000 */
#define ESPLINK_PARAM_V12_OFFSET_MV        6u   /* i32, mV,       def 0,    -2000..2000 */
#define ESPLINK_PARAM_FILTER_MEDIAN_SIZE   7u   /* u32, samples,  def 3,    1/3/5, 1=bypass */
#define ESPLINK_PARAM_FILTER_AVERAGE_WINDOW 8u  /* u32, samples,  def 10,   1..10, 1=bypass */
#define ESPLINK_PARAM_CHG_EFF_UP_PERMILLE  9u   /* u32, permille, def 758,  100..999   */
#define ESPLINK_PARAM_CHG_EFF_DN_PERMILLE  10u  /* u32, permille, def 242,  100..999   */
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
#define ESPLINK_PARAM_COUNT                20u

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
 * @brief  [EN] Send one telemetry frame and consume every received command
 *              frame. No STM command protocol yet -> replaced by the full
 *              engine (user order 2026-09-22).
 *         [FA] یک فریم تله‌متری می‌فرستد و همهٔ فریم‌های فرمان دریافتی را
 *              مصرف می‌کند (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 * @param  app_state_t__state [EN] System state / حالت سیستم
 * @param  fault_mask_t__faults [EN] Fault bits / بیت‌های خطا
 */
void func__EspLink_Run(const measurement_snapshot_t *measurement_snapshot_t__snap,
                       app_state_t app_state_t__state,
                       fault_mask_t fault_mask_t__faults);

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 * @param  bool__on [EN] true=on, false=off / روشن/خاموش
 */
void func__EspLink_Power(bool bool__on);

#endif /* ESP_LINK_H */
