/**
 * @file    esp_link.c
 * @brief   [EN] ESP-Link engine: power control, byte-frame parser, parameter
 *              dispatch and periodic telemetry over the logical UART port
 *              (user order 2026-09-22). Pure software, static allocation
 *              only, no RTOS call inside the parser.
 *          [FA] موتور ‎ESP-Link‎: کنترل تغذیه، پارسر فریم بایتی، اعمال
 *              پارامترها و تله‌متری دوره‌ای روی پورت منطقی UART (دستور
 *              کاربر ۲۰۲۶-۰۹-۲۲). تماماً نرم‌افزار، فقط تخصیص ایستا، بدون
 *              فراخوانی RTOS داخل پارسر.
 */

#include "esp_link.h"
#include "app_config.h"
#include "modules_enable.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"
#include "bsp_measurement.h"
#include "esp_link_nvm.h"
#include "cal_lut.h"

#include <stddef.h>

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_CHARGER
#include "charger.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif
#if MODULE_UI
#include "ui_led.h"
#endif
#if MODULE_IMBALANCE
#include "imbalance.h"
#endif

/* ==================== Parser state / وضعیت پارسر ==================== */

/**
 * @brief  [EN] Parser states of the receive state machine.
 *         [FA] وضعیت‌های پارسر ماشین حالت دریافت.
 */
typedef enum
{
    ESP_LINK_PARSE_WAIT_SOF0 = 0,
    ESP_LINK_PARSE_WAIT_SOF1,
    ESP_LINK_PARSE_WAIT_VERSION,
    ESP_LINK_PARSE_WAIT_TYPE,
    ESP_LINK_PARSE_WAIT_LEN_LO,
    ESP_LINK_PARSE_WAIT_LEN_HI,
    ESP_LINK_PARSE_WAIT_PAYLOAD,
    ESP_LINK_PARSE_WAIT_CRC_LO,
    ESP_LINK_PARSE_WAIT_CRC_HI
} esp_link_parse_state_t;

static esp_link_parse_state_t ESP_LINK_PARSE_STATE_T__G__State =
    ESP_LINK_PARSE_WAIT_SOF0;
static uint8_t UINT8_T__G__FrameType;
static uint16_t UINT16_T__G__FrameLen;
static uint16_t UINT16_T__G__PayloadIndex;
static uint8_t UINT8_T__G__PayloadBuffer[ESPLINK_FRAME_MAX_PAYLOAD];
static uint16_t UINT16_T__G__Crc;
static uint8_t  UINT8_T__G__RxCrcLow;
/* [EN] Link health counters. A CRC error used to be indistinguishable from a
   quiet link; now both conditions are countable, and a version mismatch says
   plainly that the two boards were flashed out of step.
   [FA] شمارنده‌های سلامت لینک. قبلاً خطای CRC از لینک ساکت قابل تشخیص نبود. */
static uint32_t UINT32_T__G__RxCrcError;
static uint32_t UINT32_T__G__RxVersionMismatch;
/* [EN] A reset is authorized only by the immediately preceding successful
   LUT_COMMIT acknowledgment. A table being active is not enough: it may be
   an old table, and a replayed LUT_RESET must not reboot a live charger.
   BEGIN/CHUNK/failed COMMIT invalidate the authorization; accepting RESET
   consumes it, so the same reset frame cannot be replayed.
   [FA] ریست فقط با آخرین ACK موفق LUT_COMMIT مجاز است. فعال‌بودن جدول به‌تنهایی
   کافی نیست چون ممکن است جدول قدیمی باشد و replay شدن LUT_RESET نباید شارژر
   زنده را ریبوت کند. ‎BEGIN/CHUNK/COMMIT‎ ناموفق مجوز را باطل می‌کنند و پذیرش
   RESET آن را مصرف می‌کند تا همان فریم دوباره قابل استفاده نباشد. */
static bool BOOL__G__LutResetAuthorized = false;
/* [EN] Point counts declared by the BEGIN of the current transaction,
   echoed in EVERY LUT_ACK so the ESP can match each ACK against the push
   it just sent. User board bug 2026-10-07: the ACK echoed
   func__CalLut_Points() - the ACTIVE table - which is 0 on a fresh board,
   so the ESP rejected the first push's perfectly good ACKs, retried three
   times and reported "board did not answer the send stage" although the
   board answered every frame.
   [FA] تعداد نقاطی که BEGIN همین تراکنش اعلام کرده و در هر LUT_ACK
   بازپس داده می‌شود تا ESP تأیید را با ارسال خودش مطابقت دهد. باگ برد
   کاربر ۲۰۲۶-۱۰-۰۷: ACK قبلاً تعداد جدول «فعال» را می‌گفت که در برد نو
   صفر است؛ ESP تأیید سالم اولین ارسال را رد می‌کرد و با سه تلاش مجدد،
   «برد به مرحلهٔ ارسال پاسخ نداد» گزارش می‌شد درحالی‌که برد پاسخ داده بود. */
static uint8_t UINT8_T__G__LutStageN1 = 0u;
static uint8_t UINT8_T__G__LutStageN2 = 0u;
#ifdef ESPLINK_HOST_TEST
static uint32_t UINT32_T__G__HostAcceptedFrames = 0u;
#endif
static uint16_t UINT16_T__G__TelemetrySeq = 0u;

/* [EN] TLM_LIVE flag bits (payload offset 2). / [FA] بیت‌های پرچم TLM_LIVE. */
#define ESPLINK_TLM_FLAG_SNAP_VALID     0x01u
#define ESPLINK_TLM_FLAG_INPUT_PRESENT  0x02u
#define ESPLINK_TLM_FLAG_MEAS_VALID     0x04u
#define ESPLINK_TLM_FLAG_CHG1_ENABLE    0x08u
#define ESPLINK_TLM_FLAG_CHG2_ENABLE    0x10u
/* [EN] v1.2 (user order 2026-09-23): manual test mode active on the STM.
 * [FA] v1.2 (دستور کاربر): مود تست دستی روی برد فعال است. */
#define ESPLINK_TLM_FLAG_MANUAL_MODE    0x20u

/* [EN] TLM byte 3 (formerly "reserved") since v1.43: imbalance scenario 5
 *      status bits so the panel can draw the live face without a GET.
 * [FA] بایت ۳ فریم TLM از نسخهٔ ۱٫۴۳: بیت‌های وضعیت سناریوی ۵. */
#define ESPLINK_TLM_FLAG2_IMBAL_EPISODE   0x01u
#define ESPLINK_TLM_FLAG2_IMBAL_LATCHED   0x02u
#define ESPLINK_TLM_FLAG2_IMBAL_BLOCK_OUT 0x04u
#define ESPLINK_TLM_FLAG2_IMBAL_NO_CHARGE 0x08u

/* ==================== Byte packing / بسته‌بندی بایت ==================== */

/**
 * @brief  [EN] Append one little-endian u16 at the cursor and advance it.
 *         [FA] یک u16 اندیان‌کوچک در مکان cursor می‌نویسد و جلو می‌برد.
 * @param  uint8_t__buffer [EN] Frame buffer / بافر فریم
 * @param  uint16_t__cursor [EN] Write cursor / مکان نوشتن
 * @param  uint16_t__value [EN] Value / مقدار
 */
static void func__EspLink_PutU16(uint8_t *uint8_t__buffer,
                                 uint16_t *uint16_t__cursor,
                                 uint16_t uint16_t__value)
{
    uint8_t__buffer[*uint16_t__cursor] = (uint8_t)(uint16_t__value & 0xFFu);
    uint8_t__buffer[*uint16_t__cursor + 1u] = (uint8_t)((uint16_t__value >> 8) & 0xFFu);
    *uint16_t__cursor = (uint16_t)(*uint16_t__cursor + 2u);
}

/**
 * @brief  [EN] Append one little-endian u32 at the cursor and advance it.
 *         [FA] یک u32 اندیان‌کوچک در مکان cursor می‌نویسد و جلو می‌برد.
 * @param  uint8_t__buffer [EN] Frame buffer / بافر فریم
 * @param  uint16_t__cursor [EN] Write cursor / مکان نوشتن
 * @param  uint32_t__value [EN] Value / مقدار
 */
static void func__EspLink_PutU32(uint8_t *uint8_t__buffer,
                                 uint16_t *uint16_t__cursor,
                                 uint32_t uint32_t__value)
{
    uint8_t__buffer[*uint16_t__cursor] = (uint8_t)(uint32_t__value & 0xFFu);
    uint8_t__buffer[*uint16_t__cursor + 1u] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
    uint8_t__buffer[*uint16_t__cursor + 2u] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
    uint8_t__buffer[*uint16_t__cursor + 3u] = (uint8_t)((uint32_t__value >> 24) & 0xFFu);
    *uint16_t__cursor = (uint16_t)(*uint16_t__cursor + 4u);
}

/**
 * @brief  [EN] Read one little-endian u32 from a payload buffer.
 *         [FA] یک u32 اندیان‌کوچک از بافر payload می‌خواند.
 * @param  uint8_t__payload [EN] Payload buffer / بافر payload
 * @param  uint8_t__offset [EN] Byte offset, 0..ESPLINK_FRAME_MAX_PAYLOAD-4‎ / آفست بایتی
 * @note   [EN] AUDIT 2026-10-05: the offset was uint8_t while the LUT_CHUNK
 *         loop computes 3 + 8*i against a 512-byte payload ceiling, so a
 *         frame claiming more than 31 points wrapped the offset and read the
 *         wrong bytes. Unreachable today (the panel caps at 24 points and
 *         CalLut rejects index >= 24), hardened anyway: a parser must not
 *         depend on a sender's good manners.
 *         [FA] ممیزی ۲۰۲۶-۱۰-۰۵: آفست ‎uint8_t‎ بود ولی حلقهٔ ‎LUT_CHUNK‎ با
 *         سقف ۵۱۲ بایتی ‎3 + 8*i‎ را می‌سازد؛ فریمی با بیش از ۳۱ نقطه آفست را
 *         می‌پیچاند و بایت اشتباه می‌خواند. امروز دست‌نیافتنی است (پنل سقف ۲۴
 *         نقطه دارد) ولی سخت‌تر شد: پارسر نباید به ادب فرستنده تکیه کند.
 * @return uint32_t [EN] Value / مقدار
 */
static uint32_t func__EspLink_GetU32(const uint8_t *uint8_t__payload,
                                     uint16_t uint8_t__offset)
{
    return ((uint32_t)uint8_t__payload[uint8_t__offset] |
            ((uint32_t)uint8_t__payload[uint8_t__offset + 1u] << 8) |
            ((uint32_t)uint8_t__payload[uint8_t__offset + 2u] << 16) |
            ((uint32_t)uint8_t__payload[uint8_t__offset + 3u] << 24));
}

/* ==================== Parameter dispatch / اعمال پارامترها ==================== */

/**
 * @brief  [EN] Apply one parameter value through the owning module setter
 *              (every setter clamps) and report the applied value back.
 *              Returns false for an unknown id, so no report frame is sent.
 *         [FA] یک مقدار پارامتر را از طریق setter ماژول مالکش اعمال
 *              می‌کند (همهٔ setter ها گیره می‌زنند) و مقدار اعمال‌شده را
 *              برمی‌گرداند. برای id ناشناخته false می‌دهد و فریمی ارسال
 *              نمی‌شود.
 * @param  uint8_t__paramId [EN] Parameter id / شناسهٔ پارامتر
 * @param  uint32_t__value [EN] Raw wire value (signed fields arrive as
 *                              two's complement) / مقدار خام خط
 * @param  uint32_t *uint32_t__appliedValue [EN] Applied value out‎ / اعمال‌شده
 * @return bool [EN] true when the id is known / id‎ شناخته شد
 */
bool func__EspLink_ApplyParam(uint8_t uint8_t__paramId,
                                     uint32_t uint32_t__value,
                                     uint32_t *uint32_t__appliedValue)
{
    if (uint32_t__appliedValue == NULL)
    {
        return false;
    }

    switch (uint8_t__paramId)
    {
        case ESPLINK_PARAM_CUR1_OFFSET_COUNTS:
            *uint32_t__appliedValue =
                func__BspMeasurement_SetCurrentOffsetCounts(0u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_CUR2_OFFSET_COUNTS:
            *uint32_t__appliedValue =
                func__BspMeasurement_SetCurrentOffsetCounts(1u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_CUR1_GAIN_PERMILLE:
            *uint32_t__appliedValue =
                func__BspMeasurement_SetCurrentGainPermille(0u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_CUR2_GAIN_PERMILLE:
            *uint32_t__appliedValue =
                func__BspMeasurement_SetCurrentGainPermille(1u, uint32_t__value);
            return true;

#if MODULE_MEASUREMENT
        case ESPLINK_PARAM_VIN_OFFSET_MV:
            *uint32_t__appliedValue = (uint32_t)func__Measurement_SetVoltageOffsetMv(
                0u, (int32_t)uint32_t__value);
            return true;

        case ESPLINK_PARAM_V24_OFFSET_MV:
            *uint32_t__appliedValue = (uint32_t)func__Measurement_SetVoltageOffsetMv(
                1u, (int32_t)uint32_t__value);
            return true;

        case ESPLINK_PARAM_V12_OFFSET_MV:
            *uint32_t__appliedValue = (uint32_t)func__Measurement_SetVoltageOffsetMv(
                2u, (int32_t)uint32_t__value);
            return true;

        case ESPLINK_PARAM_FILTER_MEDIAN_SIZE:
            *uint32_t__appliedValue = (uint32_t)func__Measurement_SetFilterMedianSize(
                (uint8_t)(uint32_t__value & 0xFFu));
            return true;

        case ESPLINK_PARAM_FILTER_AVERAGE_WINDOW:
            *uint32_t__appliedValue = (uint32_t)func__Measurement_SetFilterAverageWindow(
                uint32_t__value);
            return true;
#endif

#if MODULE_CHARGER
        case ESPLINK_PARAM_CHG_ETA1_PERMILLE:
            *uint32_t__appliedValue =
                func__Charger_SetEfficiencyPermille(0u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_CHG_ETA2_PERMILLE:
            *uint32_t__appliedValue =
                func__Charger_SetEfficiencyPermille(1u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_CHG1_ENABLE:
            func__Charger_SetChannelEspEnable(0u, uint32_t__value != 0u);
            *uint32_t__appliedValue =
                (func__Charger_GetChannelEspEnable(0u) != false) ? 1u : 0u;
            return true;

        case ESPLINK_PARAM_CHG2_ENABLE:
            func__Charger_SetChannelEspEnable(1u, uint32_t__value != 0u);
            *uint32_t__appliedValue =
                (func__Charger_GetChannelEspEnable(1u) != false) ? 1u : 0u;
            return true;

        case ESPLINK_PARAM_CHG1_DUTY_CEILING:
            *uint32_t__appliedValue =
                func__Charger_SetDutyCeilingPermille(0u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_CHG2_DUTY_CEILING:
            *uint32_t__appliedValue =
                func__Charger_SetDutyCeilingPermille(1u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_CHG1_DUTY_FIXED_ON:
            func__Charger_SetDutyFixedEnable(0u, uint32_t__value != 0u);
            *uint32_t__appliedValue =
                (func__Charger_GetDutyFixedEnable(0u) != false) ? 1u : 0u;
            return true;

        case ESPLINK_PARAM_CHG1_DUTY_FIXED_VAL:
            *uint32_t__appliedValue =
                func__Charger_SetDutyFixedPermille(0u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_CHG2_DUTY_FIXED_ON:
            func__Charger_SetDutyFixedEnable(1u, uint32_t__value != 0u);
            *uint32_t__appliedValue =
                (func__Charger_GetDutyFixedEnable(1u) != false) ? 1u : 0u;
            return true;

        case ESPLINK_PARAM_CHG2_DUTY_FIXED_VAL:
            *uint32_t__appliedValue =
                func__Charger_SetDutyFixedPermille(1u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_MANUAL_TEST_MODE:
            func__Charger_SetManualTestMode(uint32_t__value != 0u);
            *uint32_t__appliedValue =
                (func__Charger_GetManualTestMode() != false) ? 1u : 0u;
            return true;

        /* [EN] Charge profile, ids 20..26 (v1.12, user order 2026-09-25):
                one shared profile for both channels; the charger clamps
                the value and re-clamps every dependent, the APPLIED value
                is reported back.
           [FA] پروفایل شارژ، شناسه‌های ۲۰..۲۶ (v1.12، دستور کاربر
                ۲۰۲۶-۰۹-۲۵): یک پروفایل مشترک برای هر دو کانال؛ شارژر مقدار
                را گیره می‌زند و همهٔ وابسته‌ها را دوباره گیره می‌زند و
                مقدار «اعمال‌شده» پاس داده می‌شود. */
        case ESPLINK_PARAM_CHG_PROFILE_ABSORB_MV:
        case ESPLINK_PARAM_CHG_PROFILE_ABSORB_ENTER_MV:
        case ESPLINK_PARAM_CHG_PROFILE_ABSORB_OVER_MV:
        case ESPLINK_PARAM_CHG_PROFILE_FLOAT_MV:
        case ESPLINK_PARAM_CHG_PROFILE_REENTRY_MV:
        case ESPLINK_PARAM_CHG_PROFILE_BULK_CURRENT_MAX_MA:
        case ESPLINK_PARAM_CHG_PROFILE_TAPER_CURRENT_MA:
            return func__Charger_SetProfileParam(uint8_t__paramId,
                                                 uint32_t__value,
                                                 uint32_t__appliedValue);

        /* [EN] Charger alarms, ids 35..37 (v1.15, user order 2026-09-26):
                down-only safety ceilings; same clamp + applied pattern.
           [FA] آلارم‌های شارژر، شناسه‌های ۳۵..۳۷ (v1.15، دستور کاربر
                ۲۰۲۶-۰۹-۲۶): سقف‌های ایمنی فقط-پایین؛ همان الگوی گیره و
                مقدار اعمال‌شده. */
        case ESPLINK_PARAM_CHG_ALARM_HARD_CURRENT_MA:
        case ESPLINK_PARAM_CHG_ALARM_OV_CUTOFF_MV:
        case ESPLINK_PARAM_CHG_ALARM_VALID_FLOOR_MV:
            return func__Charger_SetAlarmParam(uint8_t__paramId,
                                               uint32_t__value,
                                               uint32_t__appliedValue);
#endif
#if MODULE_FAULT
        /* [EN] Fault alarms, ids 27..34 (v1.15): battery/input supervision
                thresholds; the fault module clamps the whole set.
           [FA] آلارم‌های فالت، شناسه‌های ۲۷..۳۴ (v1.15): آستانه‌های نظارت
                باتری/ورودی؛ ماژول فالت کل مجموعه را گیره می‌زند. */
        case ESPLINK_PARAM_FAULT_ALARM_DISCONNECT_MV:
        case ESPLINK_PARAM_FAULT_ALARM_DISCONNECT_DEB_MS:
        case ESPLINK_PARAM_FAULT_ALARM_ABSENT_MV:
        case ESPLINK_PARAM_FAULT_ALARM_BACK_MV:
        case ESPLINK_PARAM_FAULT_ALARM_ABSENT_DEB_MS:
        case ESPLINK_PARAM_FAULT_ALARM_RECOVER_DEB_MS:
        case ESPLINK_PARAM_FAULT_ALARM_INPUT_MIN_MV:
        case ESPLINK_PARAM_FAULT_ALARM_INPUT_MAX_MV:
            return func__Fault_SetAlarmParam(uint8_t__paramId,
                                             uint32_t__value,
                                             uint32_t__appliedValue);
#endif

        default:
#if MODULE_CHARGER
            /* [EN] Two-loop CC/CV PID, ids 83..92 (v1.24): range-dispatched
                    for the same reason as the UI block - fifteen case
                    labels buy nothing; Set re-validates and clamps.
               [FA] PID دوحلقه‌ای، شناسه‌های ۸۳..۹۲ (v1.23): دیسپچ بازه‌ای
                    به همان دلیل بلوک UI؛ ستر خودش دوباره اعتبارسنجی و گیره
                    می‌کند. */
            if ((uint8_t__paramId >= CHG_PID_PARAM_CURRENT_KP) &&
                (uint8_t__paramId <= CHG_PID_PARAM_VOLTAGE_DOWN_RATE))
            {
                return func__Charger_SetPidParam(uint8_t__paramId,
                                                 uint32_t__value,
                                                 uint32_t__appliedValue);
            }
            /* [EN] Charger limits / backstop gains, ids 93..107 (v1.28,
                    user order): same range dispatch; the setter owns the
                    clamp window table.
               [FA] حدها و گین‌های پشتیبان شارژر، ۹۳..۱۰۷ (دستور کاربر):
                    همان دیسپچ بازه‌ای؛ جدول پنجرهٔ گیره مال ستر است. */
            if ((uint8_t__paramId >= CHG_LIMIT_PARAM_FIRST_ID) &&
                (uint8_t__paramId <= CHG_LIMIT_PARAM_LAST_ID))
            {
                return func__Charger_SetLimitParam(uint8_t__paramId,
                                                   uint32_t__value,
                                                   uint32_t__appliedValue);
            }
#endif
#if MODULE_UI
            /* [EN] UI cadence, ids 38..82 (v1.16 + v1.17 append) plus the charge-side percent map 119/120 (v1.49): range-dispatched -
                    45 case labels would drown the switch; Set re-validates.
               [FA] اعداد UI، شناسه‌های ۳۸..۸۲: دیسپچ بازه‌ای. */
            if (((uint8_t__paramId >= UI_ALARM_PARAM_MIN_ID) &&
                 (uint8_t__paramId <= UI_ALARM_PARAM_MAX_ID)) ||
                 ((uint8_t__paramId >= UI_ALARM_PARAM_EXT_MIN_ID) &&
                 (uint8_t__paramId <= UI_ALARM_PARAM_EXT_MAX_ID)) ||
                ((uint8_t__paramId >= UI_ALARM_PARAM_TECH_EXT_MIN_ID) &&
                 (uint8_t__paramId <= UI_ALARM_PARAM_TECH_EXT_MAX_ID)))
            {
                return func__Ui_SetAlarmParam(uint8_t__paramId,
                                              uint32_t__value,
                                              uint32_t__appliedValue);
            }
#endif
#if MODULE_IMBALANCE
            /* [EN] Imbalance scenario 5, ids 108..118, 123..124, 132..133
                    and clean-FLOAT-cycle threshold 136 + runtime slots
                    200..202 (NVM boot replay only; the panel never sends
                    those slots, and they are never part of a backup).
               [FA] سناریوی ۵ عدم‌توازن، ۱۰۸..۱۱۸، ۱۲۳..۱۲۴، ۱۳۲..۱۳۳ و
                    آستانهٔ سیکل پاک ۱۳۶ + اسلات‌های ۲۰۰..۲۰۲ (فقط پخش NVM
                    هنگام بوت). */
            if (IMBAL_PARAM_OWNS(uint8_t__paramId) ||
                ((uint8_t__paramId >= IMBAL_SLOT_FIRST_ID) &&
                 (uint8_t__paramId <= IMBAL_SLOT_LAST_ID)))
            {
                return func__Imbalance_SetParam(uint8_t__paramId,
                                                uint32_t__value,
                                                uint32_t__appliedValue);
            }
#endif
#if MODULE_CHARGER
            /* [EN] v1.82 scenario 6 (dead battery), ids 125..131 and 134..135 +
                    runtime slot 203 (NVM boot replay of the latch mask).
               [FA] سناریوی ۶ باتری خراب، ۱۲۵..۱۳۱ و ۱۳۴..۱۳۵ + اسلات ۲۰۳. */
            if (CHG_DEAD_PARAM_OWNS(uint8_t__paramId) ||
                (uint8_t__paramId == CHG_DEAD_SLOT_MASK_ID))
            {
                return func__Charger_SetDeadParam(uint8_t__paramId,
                                                  uint32_t__value,
                                                  uint32_t__appliedValue);
            }
#endif
            return false;
    }
}

/**
 * @brief  [EN] Read the live value of one parameter for the report frames.
 *         [FA] مقدار زندهٔ یک پارامتر برای فریم‌های گزارش می‌خواند.
 * @param  uint8_t__paramId [EN] Parameter id / شناسهٔ پارامتر
 * @param  uint32_t *uint32_t__value [EN] Live value out‎ / مقدار زنده
 * @return bool [EN] true when the id is known / id‎ شناخته شد
 */
bool func__EspLink_GetParam(uint8_t uint8_t__paramId,
                                   uint32_t *uint32_t__value)
{
    if (uint32_t__value == NULL)
    {
        return false;
    }

    switch (uint8_t__paramId)
    {
        case ESPLINK_PARAM_CUR1_OFFSET_COUNTS:
            *uint32_t__value = func__BspMeasurement_GetCurrentOffsetCounts(0u);
            return true;

        case ESPLINK_PARAM_CUR2_OFFSET_COUNTS:
            *uint32_t__value = func__BspMeasurement_GetCurrentOffsetCounts(1u);
            return true;

        case ESPLINK_PARAM_CUR1_GAIN_PERMILLE:
            *uint32_t__value = func__BspMeasurement_GetCurrentGainPermille(0u);
            return true;

        case ESPLINK_PARAM_CUR2_GAIN_PERMILLE:
            *uint32_t__value = func__BspMeasurement_GetCurrentGainPermille(1u);
            return true;

#if MODULE_MEASUREMENT
        case ESPLINK_PARAM_VIN_OFFSET_MV:
            *uint32_t__value =
                (uint32_t)func__Measurement_GetVoltageOffsetMv(0u);
            return true;

        case ESPLINK_PARAM_V24_OFFSET_MV:
            *uint32_t__value =
                (uint32_t)func__Measurement_GetVoltageOffsetMv(1u);
            return true;

        case ESPLINK_PARAM_V12_OFFSET_MV:
            *uint32_t__value =
                (uint32_t)func__Measurement_GetVoltageOffsetMv(2u);
            return true;

        case ESPLINK_PARAM_FILTER_MEDIAN_SIZE:
            *uint32_t__value = (uint32_t)func__Measurement_GetFilterMedianSize();
            return true;

        case ESPLINK_PARAM_FILTER_AVERAGE_WINDOW:
            *uint32_t__value = (uint32_t)func__Measurement_GetFilterAverageWindow();
            return true;
#endif

#if MODULE_CHARGER
        case ESPLINK_PARAM_CHG_ETA1_PERMILLE:
            *uint32_t__value = func__Charger_GetEfficiencyPermille(0u);
            return true;

        case ESPLINK_PARAM_CHG_ETA2_PERMILLE:
            *uint32_t__value = func__Charger_GetEfficiencyPermille(1u);
            return true;

        case ESPLINK_PARAM_CHG1_ENABLE:
            *uint32_t__value =
                (func__Charger_GetChannelEspEnable(0u) != false) ? 1u : 0u;
            return true;

        case ESPLINK_PARAM_CHG2_ENABLE:
            *uint32_t__value =
                (func__Charger_GetChannelEspEnable(1u) != false) ? 1u : 0u;
            return true;

        case ESPLINK_PARAM_CHG1_DUTY_CEILING:
            *uint32_t__value = func__Charger_GetDutyCeilingPermille(0u);
            return true;

        case ESPLINK_PARAM_CHG2_DUTY_CEILING:
            *uint32_t__value = func__Charger_GetDutyCeilingPermille(1u);
            return true;

        case ESPLINK_PARAM_CHG1_DUTY_FIXED_ON:
            *uint32_t__value =
                (func__Charger_GetDutyFixedEnable(0u) != false) ? 1u : 0u;
            return true;

        case ESPLINK_PARAM_CHG1_DUTY_FIXED_VAL:
            *uint32_t__value = func__Charger_GetDutyFixedPermille(0u);
            return true;

        case ESPLINK_PARAM_CHG2_DUTY_FIXED_ON:
            *uint32_t__value =
                (func__Charger_GetDutyFixedEnable(1u) != false) ? 1u : 0u;
            return true;

        case ESPLINK_PARAM_CHG2_DUTY_FIXED_VAL:
            *uint32_t__value = func__Charger_GetDutyFixedPermille(1u);
            return true;

        case ESPLINK_PARAM_MANUAL_TEST_MODE:
            *uint32_t__value =
                (func__Charger_GetManualTestMode() != false) ? 1u : 0u;
            return true;

        /* [EN] Charge profile live read, ids 20..26 (v1.12).
           [FA] خواندن زندهٔ پروفایل شارژ، شناسه‌های ۲۰..۲۶. */
        case ESPLINK_PARAM_CHG_PROFILE_ABSORB_MV:
        case ESPLINK_PARAM_CHG_PROFILE_ABSORB_ENTER_MV:
        case ESPLINK_PARAM_CHG_PROFILE_ABSORB_OVER_MV:
        case ESPLINK_PARAM_CHG_PROFILE_FLOAT_MV:
        case ESPLINK_PARAM_CHG_PROFILE_REENTRY_MV:
        case ESPLINK_PARAM_CHG_PROFILE_BULK_CURRENT_MAX_MA:
        case ESPLINK_PARAM_CHG_PROFILE_TAPER_CURRENT_MA:
            return func__Charger_GetProfileParam(uint8_t__paramId,
                                                 uint32_t__value);

        case ESPLINK_PARAM_CHG_ALARM_HARD_CURRENT_MA:
        case ESPLINK_PARAM_CHG_ALARM_OV_CUTOFF_MV:
        case ESPLINK_PARAM_CHG_ALARM_VALID_FLOOR_MV:
            return func__Charger_GetAlarmParam(uint8_t__paramId,
                                               uint32_t__value);
#endif
#if MODULE_FAULT
        case ESPLINK_PARAM_FAULT_ALARM_DISCONNECT_MV:
        case ESPLINK_PARAM_FAULT_ALARM_DISCONNECT_DEB_MS:
        case ESPLINK_PARAM_FAULT_ALARM_ABSENT_MV:
        case ESPLINK_PARAM_FAULT_ALARM_BACK_MV:
        case ESPLINK_PARAM_FAULT_ALARM_ABSENT_DEB_MS:
        case ESPLINK_PARAM_FAULT_ALARM_RECOVER_DEB_MS:
        case ESPLINK_PARAM_FAULT_ALARM_INPUT_MIN_MV:
        case ESPLINK_PARAM_FAULT_ALARM_INPUT_MAX_MV:
            return func__Fault_GetAlarmParam(uint8_t__paramId,
                                             uint32_t__value);
#endif

        default:
#if MODULE_CHARGER
            /* [EN] Two-loop CC/CV PID live read, ids 83..92 (v1.23).
               [FA] خواندن زندهٔ PID دوحلقه‌ای، شناسه‌های ۸۳..۹۲. */
            if ((uint8_t__paramId >= CHG_PID_PARAM_CURRENT_KP) &&
                (uint8_t__paramId <= CHG_PID_PARAM_VOLTAGE_DOWN_RATE))
            {
                return func__Charger_GetPidParam(uint8_t__paramId,
                                                 uint32_t__value);
            }
            /* [EN] Charger limits / backstop gains live read, ids 93..107.
               [FA] خواندن زندهٔ حدها و گین‌های پشتیبان، ۹۳..۱۰۷. */
            if ((uint8_t__paramId >= CHG_LIMIT_PARAM_FIRST_ID) &&
                (uint8_t__paramId <= CHG_LIMIT_PARAM_LAST_ID))
            {
                return func__Charger_GetLimitParam(uint8_t__paramId,
                                                   uint32_t__value);
            }
#endif
#if MODULE_UI
            /* [EN] UI cadence live read, ids 38..82 plus the charge map 119/120 (v1.49).
               [FA] خواندن زندهٔ اعداد UI، شناسه‌های ۳۸..۸۲. */
            if (((uint8_t__paramId >= UI_ALARM_PARAM_MIN_ID) &&
                 (uint8_t__paramId <= UI_ALARM_PARAM_MAX_ID)) ||
                 ((uint8_t__paramId >= UI_ALARM_PARAM_EXT_MIN_ID) &&
                 (uint8_t__paramId <= UI_ALARM_PARAM_EXT_MAX_ID)) ||
                ((uint8_t__paramId >= UI_ALARM_PARAM_TECH_EXT_MIN_ID) &&
                 (uint8_t__paramId <= UI_ALARM_PARAM_TECH_EXT_MAX_ID)))
            {
                return func__Ui_GetAlarmParam(uint8_t__paramId,
                                              uint32_t__value);
            }
#endif
#if MODULE_IMBALANCE
            /* [EN] Imbalance live read, ids 108..118 + slots 200..202
                    (the NVM save snapshots its persisted set through here).
               [FA] خواندن زندهٔ عدم‌توازن + اسلات‌ها (برداشت NVM). */
            if (IMBAL_PARAM_OWNS(uint8_t__paramId) ||
                ((uint8_t__paramId >= IMBAL_SLOT_FIRST_ID) &&
                 (uint8_t__paramId <= IMBAL_SLOT_LAST_ID)))
            {
                return func__Imbalance_GetParam(uint8_t__paramId,
                                                uint32_t__value);
            }
#endif
#if MODULE_CHARGER
            /* [EN] v1.82 scenario 6 live read, ids 125..131 and 134..135 + slot 203.
               [FA] خواندن زندهٔ سناریوی ۶، ۱۲۵..۱۳۱ و ۱۳۴..۱۳۵ + اسلات ۲۰۳. */
            if (CHG_DEAD_PARAM_OWNS(uint8_t__paramId) ||
                (uint8_t__paramId == CHG_DEAD_SLOT_MASK_ID))
            {
                return func__Charger_GetDeadParam(uint8_t__paramId,
                                                  uint32_t__value);
            }
#endif
            return false;
    }
}

/* ==================== Frame transmit / ارسال فریم ==================== */

/* ==================== CRC-16 ==================== */
/**
 * @brief  [EN] CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no
 *              final xor). Bitwise on purpose: a 256-entry table would cost
 *              512 bytes of flash on a part that does not have it to spare, and
 *              these frames are at most ~470 bytes at 10 Hz, so the loop is far
 *              cheaper than the table.
 *         [‎FA] CRC-16/CCITT-FALSE‎. عمداً بیتی: جدول ۲۵۶تایی ۵۱۲ بایت فلش می‌خواهد
 *              روی قطعه‌ای که این فضا را ندارد، و این فریم‌ها حداکثر ~۴۷۰ بایت با
 *              نرخ ۱۰ هرتز‌اند، پس حلقه از جدول خیلی ارزان‌تر است.
 * @param  uint16_t__crc  [EN] Running value / مقدار جاری
 * @param  uint8_t__byte  [EN] Next byte / بایت بعدی
 * @return uint16_t [EN] Updated CRC / CRC‎ به‌روزشده
 */
static uint16_t func__EspLink_Crc16(uint16_t uint16_t__crc, uint8_t uint8_t__byte)
{
    uint8_t uint8_t__bit;

    uint16_t__crc = (uint16_t)(uint16_t__crc ^ ((uint16_t)uint8_t__byte << 8));
    for (uint8_t__bit = 0u; uint8_t__bit < 8u; uint8_t__bit++)
    {
        if ((uint16_t__crc & 0x8000u) != 0u)
        {
            uint16_t__crc = (uint16_t)(((uint16_t)(uint16_t__crc << 1)) ^
                                       (uint16_t)ESPLINK_CRC16_POLY);
        }
        else
        {
            uint16_t__crc = (uint16_t)(uint16_t__crc << 1);
        }
    }
    return uint16_t__crc;
}

/**
 * @brief  [EN] Wrap a payload in the standard frame and transmit it.
 *         [FA] payload را در فریم استاندارد می‌پیچد و ارسال می‌کند.
 * @param  uint8_t__messageType [EN] Message type byte / بایت نوع پیام
 * @param  const uint8_t *uint8_t__payload [EN] Payload / payload
 * @param  uint16_t uint16_t__payloadLength [EN] Payload length, 0..512 (v1.16 u16 length)‎ / طول payload
 */
static void func__EspLink_SendFrame(uint8_t uint8_t__messageType,
                                    const uint8_t *uint8_t__payload,
                                    uint16_t uint16_t__payloadLength)
{
    /* [EN] STATIC by necessity, not style (v1.67 RAM audit, tools/measure_ram.py):
       this 520-byte buffer on the stack made the worst-case comm chain
       TaskComm -> EspLink_Run -> SendLutAck -> SendFrame -> BspUart_Write ->
       PumpTx -> HAL_UART_Transmit_DMA -> HAL_DMA_Start_IT reach 932 of the
       1024-byte comm stack (91%) - below the usual 70% ceiling only by luck,
       and one nested interrupt away from a silent overflow into the next
       task's stack. Only the comm task ever sends a frame and the function is
       non-reentrant (it never calls anything that calls back into it), so a
       static buffer is race-free here - the same rationale as
       SendParamsBulk's payload and the NVM save scratch. Cost: 520 bytes of
       .bss out of the 3.5 KiB that were free; gain: the chain drops to about
       410 bytes (40%).
       [FA] عمداً STATIC نه سلیقه‌ای (ممیزی رم نسخه ۱.۶۷، ابزار
       ‎tools/measure_ram.py)‎: این بافر ۵۲۰ بایتی روی پشته، بدترین زنجیرهٔ تسک
       ارتباط را به ۹۳۲ از ۱۰۲۴ بایت (۹۱٪) می‌رساند؛ یک وقفهٔ تودرتو تا سرریز
       بی‌صدا به پشتهٔ تسک بعدی فاصله داشت. فقط تسک ارتباط فریم می‌فرستد و این
       تابع بازگشتی نیست، پس static بدون مسابقه است. هزینه: ۵۲۰ بایت .bss از
       ۳٫۵ کیلوبایت آزاد؛ سود: زنجیره به حدود ۴۱۰ بایت (۴۰٪) می‌رسد. */
    static uint8_t UINT8_T__A__Frame[ESPLINK_FRAME_HEADER_SIZE +
                                     ESPLINK_FRAME_MAX_PAYLOAD +
                                     ESPLINK_FRAME_CHECKSUM_SIZE];
    uint16_t uint16_t__cursor;
    uint16_t uint16_t__crc;
    uint8_t uint8_t__lenLo;
    uint8_t uint8_t__lenHi;
    uint32_t uint32_t__i;

    if (uint16_t__payloadLength > ESPLINK_FRAME_MAX_PAYLOAD)
    {
        return;
    }

    /* [EN] v1.16: u16 little-endian length (len_lo + len_hi).
       [FA] نسخه ۱.۱۶: طول u16 لیتل‌اندین. */
    uint8_t__lenLo = (uint8_t)(uint16_t__payloadLength & 0xFFu);
    uint8_t__lenHi = (uint8_t)((uint16_t__payloadLength >> 8) & 0xFFu);

    UINT8_T__A__Frame[0] = (uint8_t)ESPLINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = (uint8_t)ESPLINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = (uint8_t)ESPLINK_PROTOCOL_VERSION;
    UINT8_T__A__Frame[3] = uint8_t__messageType;
    UINT8_T__A__Frame[4] = uint8_t__lenLo;
    UINT8_T__A__Frame[5] = uint8_t__lenHi;

    for (uint32_t__i = 0u; uint32_t__i < (uint32_t)uint16_t__payloadLength; uint32_t__i++)
    {
        UINT8_T__A__Frame[ESPLINK_FRAME_HEADER_SIZE + uint32_t__i] =
            uint8_t__payload[uint32_t__i];
    }

    /* [EN] The CRC covers version, type, both length bytes and the payload -
       everything after the SOF pair. [FA] CRC نسخه، نوع، هر دو بایت طول و
       payload را پوشش می‌دهد - هرچه بعد از جفت SOF می‌آید. */
    uint16_t__crc = (uint16_t)ESPLINK_CRC16_INIT;
    uint16_t__crc = func__EspLink_Crc16(uint16_t__crc, (uint8_t)ESPLINK_PROTOCOL_VERSION);
    uint16_t__crc = func__EspLink_Crc16(uint16_t__crc, uint8_t__messageType);
    uint16_t__crc = func__EspLink_Crc16(uint16_t__crc, uint8_t__lenLo);
    uint16_t__crc = func__EspLink_Crc16(uint16_t__crc, uint8_t__lenHi);
    for (uint32_t__i = 0u; uint32_t__i < (uint32_t)uint16_t__payloadLength; uint32_t__i++)
    {
        uint16_t__crc = func__EspLink_Crc16(uint16_t__crc, uint8_t__payload[uint32_t__i]);
    }

    uint16_t__cursor = (uint16_t)(ESPLINK_FRAME_HEADER_SIZE + uint16_t__payloadLength);
    UINT8_T__A__Frame[uint16_t__cursor] = (uint8_t)(uint16_t__crc & 0xFFu);
    UINT8_T__A__Frame[uint16_t__cursor + 1u] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
    uint16_t__cursor = (uint16_t)(uint16_t__cursor + ESPLINK_FRAME_CHECKSUM_SIZE);

    (void)func__BspUart_Write(UINT8_T__A__Frame, (uint16_t)uint16_t__cursor);
}

/**
 * @brief  [EN] Send one PARAM_REPORT frame (id + applied value).
 *         [FA] یک فریم PARAM_REPORT می‌فرستد (شناسه + مقدار اعمال‌شده).
 * @param  uint8_t__paramId [EN] Parameter id / شناسهٔ پارامتر
 * @param  uint32_t__appliedValue [EN] Applied value / مقدار اعمال‌شده
 */
static void func__EspLink_SendParamReport(uint8_t uint8_t__paramId,
                                          uint32_t uint32_t__appliedValue)
{
    uint8_t UINT8_T__A__Payload[5u];

    UINT8_T__A__Payload[0] = uint8_t__paramId;
    UINT8_T__A__Payload[1] = (uint8_t)(uint32_t__appliedValue & 0xFFu);
    UINT8_T__A__Payload[2] = (uint8_t)((uint32_t__appliedValue >> 8) & 0xFFu);
    UINT8_T__A__Payload[3] = (uint8_t)((uint32_t__appliedValue >> 16) & 0xFFu);
    UINT8_T__A__Payload[4] = (uint8_t)((uint32_t__appliedValue >> 24) & 0xFFu);

    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_PARAM_REPORT,
                            UINT8_T__A__Payload, 5u);
}

/**
 * @brief  [EN] Send every known parameter as PARAMS_BULK, in as many frames
 *              as it takes. The receiver stores items BY ID, so splitting
 *              the reply needs no protocol change at all.
 *         [FA] همهٔ پارامترها را در قالب PARAMS_BULK می‌فرستد، در هر چند فریم
 *              که لازم باشد. گیرنده آیتم‌ها را «با شناسه» ذخیره می‌کند، پس
 *              تکه‌کردن پاسخ هیچ تغییر پروتکلی لازم ندارد.
 */

/* [EN] v1.28: the single-frame reply hit its ceiling. One frame holds
 *      1 + N*5 bytes against ESPLINK_FRAME_MAX_PAYLOAD, i.e. at most 102
 *      parameters, and the user-ordered limits block took the count to 108.
 *      Rather than raise the payload ceiling - which costs RAM in a static
 *      buffer on both sides of a 20 KiB part - the reply is now CHUNKED.
 *      This works without touching the wire format because every item
 *      carries its own id and the ESP parser is count-driven: it already
 *      merges whatever ids arrive, in any grouping.
 *      The chunk size is DERIVED from the payload ceiling, so it can never
 *      disagree with the buffer it has to fit.
 * [FA] در v1.28 پاسخ تک‌فریمی به سقفش خورد. یک فریم ۱+۵N بایت در برابر
 *      ESPLINK_FRAME_MAX_PAYLOAD جا می‌دهد یعنی حداکثر ۱۰۲ پارامتر، و بلوک
 *      حدها به دستور کاربر تعداد را به ۱۰۸ رساند. به‌جای بالا بردن سقف
 *      payload - که روی قطعهٔ ۲۰ کیلوبایتی در هر دو سمت RAM می‌خورد - پاسخ
 *      «تکه‌تکه» می‌شود. این بدون دست‌زدن به فرمت سیم کار می‌کند چون هر آیتم
 *      شناسهٔ خودش را دارد و پارسر ESP شمارش‌محور است: همین حالا هر شناسه‌ای
 *      را در هر گروه‌بندی ادغام می‌کند. اندازهٔ تکه از همان سقف payload مشتق
 *      می‌شود تا هرگز با بافری که باید در آن جا شود اختلاف پیدا نکند. */
/* [EN] The link is the ONLY way the panel can reach the charger limit block,
       and this file is the only one that sees both names - so this is where
       the check belongs. A parameter count that stops short of the last limit
       id compiles, runs, and simply makes the tail of the block unreachable:
       the parameter is "settable" in the header and invisible in the panel.
       That is precisely the failure the user ordered removed, so it is a
       build error, not a comment. Mutation testing found this gap: lowering
       ESPLINK_PARAM_COUNT by one passed every check that existed.
   [FA] تنها راه رسیدن پنل به بلوک حدهای شارژر، همین لینک است و تنها فایلی که
       هر دو نام را می‌بیند همین است - پس جای این چک همین‌جاست. تعداد پارامتری
       که به آخرین شناسهٔ حد نرسد، کامپایل و اجرا می‌شود و فقط دُم بلوک را
       دسترس‌ناپذیر می‌کند: پارامتر در هدر «تنظیم‌شدنی» است و در پنل نامرئی.
       دقیقاً همان خرابی‌ای که کاربر دستور حذفش را داد، پس خطای بیلد است نه
       کامنت. موتیشن‌تست این شکاف را پیدا کرد: یکی کم‌کردن ESPLINK_PARAM_COUNT
       از همهٔ چک‌های موجود سالم رد می‌شد. */
/* [EN] Full-program audit 2026-10-05: the pair below names charger macros,
   so it only exists when the charger header was actually included (the
   #include above is itself inside #if MODULE_CHARGER). Without the guard,
   MODULE_CHARGER 0 turned these two asserts into undeclared-identifier
   errors instead of simply dropping a charger-only contract.
   [FA] ممیزی ۲۰۲۶-۱۰-۰۵: این جفت assert ماکروهای شارژر را نام می‌برد و فقط
   وقتی معنا دارد که هدر شارژر include شده باشد (خودِ include بالاتر داخل
   ‎#if MODULE_CHARGER‎ است). بدون گارد، خاموش‌کردن شارژر به‌جای حذف یک قرارداد
   مخصوص شارژر، خطای شناسهٔ تعریف‌نشده می‌داد. */
#if MODULE_CHARGER
_Static_assert(ESPLINK_PARAM_COUNT > CHG_LIMIT_PARAM_LAST_ID,
               "ESPLINK_PARAM_COUNT must cover the whole charger limit block");
_Static_assert(CHG_LIMIT_PARAM_FIRST_ID + CHG_LIMIT_COUNT - 1u
               == CHG_LIMIT_PARAM_LAST_ID,
               "the charger limit block has a hole in its wire-id range");
#endif /* MODULE_CHARGER */

#define ESPLINK_BULK_ITEM_SIZE   5u
#define ESPLINK_BULK_MAX_ITEMS   \
    ((ESPLINK_FRAME_MAX_PAYLOAD - 1u) / ESPLINK_BULK_ITEM_SIZE)

_Static_assert(ESPLINK_BULK_MAX_ITEMS >= 1u,
               "a bulk frame must carry at least one parameter");
_Static_assert(ESPLINK_BULK_MAX_ITEMS <= 255u,
               "the bulk count byte cannot describe more than 255 items");

static void func__EspLink_SendParamsBulk(void)
{
    /* [EN] STATIC by necessity, not style (v1.16 audit E1): on the 1 KiB
       comm stack next to SendFrame's own frame buffer this would leave only
       dozens of bytes of margin. Single task (comm), non-reentrant, so
       static is race-free here (same rationale as the NVM save scratch).
       Sized to ONE chunk now rather than to the whole parameter list, which
       is why growing the parameter count no longer grows this buffer.
       [FA] عمداً STATIC نه سلیقه‌ای: روی استک ۱KB ارتباط کنار بافر فریم خودِ
       SendFrame فقط چند ده بایت حاشیه می‌ماند؛ تک‌تسک و غیربازگشتی پس بدون
       مسابقه است. حالا به اندازهٔ «یک تکه» است نه کل فهرست پارامترها، و
       برای همین زیادشدن تعداد پارامتر دیگر این بافر را بزرگ نمی‌کند. */
    static uint8_t UINT8_T__A__Payload[1u + (ESPLINK_BULK_MAX_ITEMS *
                                             ESPLINK_BULK_ITEM_SIZE)];
    _Static_assert(sizeof(UINT8_T__A__Payload) <= ESPLINK_FRAME_MAX_PAYLOAD,
                   "PARAMS_BULK chunk must fit the protocol payload ceiling");

    uint16_t uint16_t__cursor = 1u;
    uint8_t uint8_t__count = 0u;
    uint32_t uint32_t__value;
    uint32_t uint32_t__i;

    for (uint32_t__i = 0u; uint32_t__i < ESPLINK_PARAM_COUNT; uint32_t__i++)
    {
        if (func__EspLink_GetParam((uint8_t)uint32_t__i, &uint32_t__value) == false)
        {
            continue;
        }

        UINT8_T__A__Payload[uint16_t__cursor] = (uint8_t)uint32_t__i;
        UINT8_T__A__Payload[uint16_t__cursor + 1u] = (uint8_t)(uint32_t__value & 0xFFu);
        UINT8_T__A__Payload[uint16_t__cursor + 2u] =
            (uint8_t)((uint32_t__value >> 8) & 0xFFu);
        UINT8_T__A__Payload[uint16_t__cursor + 3u] =
            (uint8_t)((uint32_t__value >> 16) & 0xFFu);
        UINT8_T__A__Payload[uint16_t__cursor + 4u] =
            (uint8_t)((uint32_t__value >> 24) & 0xFFu);
        uint16_t__cursor = (uint16_t)(uint16_t__cursor + ESPLINK_BULK_ITEM_SIZE);
        uint8_t__count++;

        /* [EN] Chunk full: ship it and start the next one.
           [FA] تکه پر شد: ارسال و شروع تکهٔ بعدی. */
        if (uint8_t__count >= ESPLINK_BULK_MAX_ITEMS)
        {
            UINT8_T__A__Payload[0] = uint8_t__count;
            func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_PARAMS_BULK,
                                    UINT8_T__A__Payload, uint16_t__cursor);
            uint16_t__cursor = 1u;
            uint8_t__count = 0u;
        }
    }

    /* [EN] Trailing partial chunk. Sent even when empty is NOT wanted - an
       empty bulk frame would tell the panel "zero parameters known".
       [FA] تکهٔ ناقص پایانی. فریم خالی عمداً فرستاده نمی‌شود چون به پنل
       می‌گوید «هیچ پارامتری شناخته نشد». */
    if (uint8_t__count > 0u)
    {
        UINT8_T__A__Payload[0] = uint8_t__count;
        func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_PARAMS_BULK,
                                UINT8_T__A__Payload, uint16_t__cursor);
    }
}

/**
 * @brief  [EN] Build and send one TLM_LIVE frame from the live globals.
 *         [FA] یک فریم TLM_LIVE از گلوبال‌های زنده می‌سازد و می‌فرستد.
 * @param  const measurement_snapshot_t *measurement_snapshot_t__snap [EN]
 *              ‎Snapshot of this pass / snapshot‎ همین پاس
 * @param  fault_mask_t fault_mask_t__faults [EN] Fault bits / بیت‌های خطا
 */
static void func__EspLink_SendTelemetry(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                        fault_mask_t fault_mask_t__faults)
{
    uint8_t UINT8_T__A__Payload[ESPLINK_TLM_PAYLOAD_SIZE];
    uint16_t uint16_t__cursor = 0u;
    uint8_t uint8_t__flags = 0u;

    if (measurement_snapshot_t__snap != NULL)
    {
        if (measurement_snapshot_t__snap->valid != false)
        {
            uint8_t__flags = (uint8_t)(uint8_t__flags | ESPLINK_TLM_FLAG_SNAP_VALID);
        }
        if (measurement_snapshot_t__snap->input_present != false)
        {
            uint8_t__flags = (uint8_t)(uint8_t__flags | ESPLINK_TLM_FLAG_INPUT_PRESENT);
        }
    }

#if MODULE_MEASUREMENT
    if (BOOL__G__MeasDataValid != false)
    {
        uint8_t__flags = (uint8_t)(uint8_t__flags | ESPLINK_TLM_FLAG_MEAS_VALID);
    }
#endif

#if MODULE_CHARGER
    if (func__Charger_GetChannelEspEnable(0u) != false)
    {
        uint8_t__flags = (uint8_t)(uint8_t__flags | ESPLINK_TLM_FLAG_CHG1_ENABLE);
    }
    if (func__Charger_GetChannelEspEnable(1u) != false)
    {
        uint8_t__flags = (uint8_t)(uint8_t__flags | ESPLINK_TLM_FLAG_CHG2_ENABLE);
    }
    if (func__Charger_IsManualTestModeActive() != false)
    {
        uint8_t__flags = (uint8_t)(uint8_t__flags | ESPLINK_TLM_FLAG_MANUAL_MODE);
    }
#endif

    func__EspLink_PutU16(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT16_T__G__TelemetrySeq);
    UINT16_T__G__TelemetrySeq = (uint16_t)(UINT16_T__G__TelemetrySeq + 1u);
    UINT8_T__A__Payload[uint16_t__cursor] = uint8_t__flags;
    uint16_t__cursor = (uint16_t)(uint16_t__cursor + 1u);
    /* [EN] v1.43: the old reserved byte now carries the imbalance scenario 5
     *      status bits (episode / latched / output-blocked / charge-halted).
     * [FA] بایت رزرو قدیمی حالا بیت‌های وضعیت سناریوی ۵ را حمل می‌کند. */
    {
        uint8_t uint8_t__imbalanceFlags = 0u;

#if MODULE_IMBALANCE
        {
            imbalance_outputs_t imbalance_outputs_t__imbalance;

            func__Imbalance_GetOutputs(&imbalance_outputs_t__imbalance);
            if (imbalance_outputs_t__imbalance.bool__episode != false)
            {
                uint8_t__imbalanceFlags |= ESPLINK_TLM_FLAG2_IMBAL_EPISODE;
            }
            if (imbalance_outputs_t__imbalance.bool__latched != false)
            {
                uint8_t__imbalanceFlags |= ESPLINK_TLM_FLAG2_IMBAL_LATCHED;
            }
            if (imbalance_outputs_t__imbalance.bool__blockOutput != false)
            {
                uint8_t__imbalanceFlags |= ESPLINK_TLM_FLAG2_IMBAL_BLOCK_OUT;
            }
            if (imbalance_outputs_t__imbalance.bool__chargingAllowed == false)
            {
                uint8_t__imbalanceFlags |= ESPLINK_TLM_FLAG2_IMBAL_NO_CHARGE;
            }
        }
#endif
        UINT8_T__A__Payload[uint16_t__cursor] = uint8_t__imbalanceFlags;
    }
    uint16_t__cursor = (uint16_t)(uint16_t__cursor + 1u);

#if MODULE_MEASUREMENT
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasCurrent1RawCounts);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasCurrent1ShuntUv);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasCurrent1MaUnfiltered);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasCurrent1Ma);
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

#if MODULE_CHARGER
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__ChargerIest1Ma);
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

#if MODULE_CHARGER
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__ChargerDiag[0u * CHG_DIAG_CHANNEL_STRIDE +
                                                  CHG_DIAG_IDX_DUTY]);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__ChargerDiag[0u * CHG_DIAG_CHANNEL_STRIDE +
                                                  CHG_DIAG_IDX_STATE]);
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

#if MODULE_MEASUREMENT
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasCurrent2RawCounts);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasCurrent2ShuntUv);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasCurrent2MaUnfiltered);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasCurrent2Ma);
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

#if MODULE_CHARGER
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__ChargerIest2Ma);
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

#if MODULE_CHARGER
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__ChargerDiag[1u * CHG_DIAG_CHANNEL_STRIDE +
                                                  CHG_DIAG_IDX_DUTY]);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__ChargerDiag[1u * CHG_DIAG_CHANNEL_STRIDE +
                                                  CHG_DIAG_IDX_STATE]);
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

#if MODULE_MEASUREMENT
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasInputVoltageMv);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasBattery24Mv);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasBattery12Mv);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasBatteryLowMv);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasBatteryHighMv);
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         (uint32_t)fault_mask_t__faults);

    /* [EN] v1.25 CALIBRATION GROUND TRUTH: raw ADC counts, before the divider
       maths, before the runtime offsets, before any bench compensation. Counts
       are the only numbers on this board that no coefficient can distort, so a
       sweep that logs these beside a DMM lets every scale be rebuilt from
       first principles rather than tuned on top of what the firmware already
       believes. VDDA rides along so the shared reference can be checked too.
       [FA] مبنای کالیبراسیون: شمارش خام ADC پیش از ریاضیات مقسم، آفست‌های زمان
       اجرا و هر جبران بنچی. شمارش تنها عددی روی این برد است که هیچ ضریبی
       خرابش نمی‌کند، پس سوییپی که این‌ها را کنار مولتی‌متر ثبت کند اجازه می‌دهد
       هر مقیاس از پایه بازساخته شود نه اینکه روی باور فعلی فرم‌ور تنظیم شود. */
    /* [EN] Full-program audit 2026-10-05: this block read the Measurement
       globals unguarded while every other Measurement block in this frame
       is wrapped, so MODULE_MEASUREMENT 0 broke the build. Guarded now with
       the same zero-fill else branch, which keeps the payload layout and
       length identical in both configurations.
       [FA] ممیزی ۲۰۲۶-۱۰-۰۵: این بلوک بدون گارد گلوبال‌های Measurement را
       می‌خواند در حالی که بقیهٔ بلوک‌های همین فریم گارد دارند، پس خاموش‌کردن
       MODULE_MEASUREMENT بیلد را می‌شکست. حالا با همان شاخهٔ صفرپرکن گارد
       شده و چیدمان و طول payload در هر دو حالت یکسان می‌ماند. */
#if MODULE_MEASUREMENT
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasVinRawCounts);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasV24RawCounts);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasV12RawCounts);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasVrefintRawCounts);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT32_T__G__MeasVddaMv);
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

    /* [EN] v1.43 imbalance live block (appended at the very end so every
     *      earlier index is untouched): |vhigh-vlow| mV, episode count,
     *      charge cycles since latch. Snap-validity is conveyed by the
     *      flags byte as usual.
     * [FA] بلوک زندهٔ عدم‌توازن (انتهای فریم، شاخص‌های قبلی دست‌نخورده). */
#if MODULE_IMBALANCE
    {
        imbalance_outputs_t imbalance_outputs_t__imbalance;

        func__Imbalance_GetOutputs(&imbalance_outputs_t__imbalance);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             imbalance_outputs_t__imbalance.uint32_t__imbalanceMv);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             imbalance_outputs_t__imbalance.uint32_t__events);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             imbalance_outputs_t__imbalance.uint32_t__latchedCycles);
    }
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

    /* [EN] v1.72 scenario 6 live block (appended after the imbalance block,
     *      again at the very end so no earlier index moves): the latched
     *      dead-battery channel mask (bit0 = ch1, bit1 = ch2) and the
     *      continuous charge time of EACH channel in seconds, which the panel
     *      draws as that battery's progress toward the 24 h verdict.
     *      v1.76 (user order: "this timer must be counted separately for each
     *      battery"): the second channel used to be hidden behind a max().
     * [FA] بلوک زندهٔ سناریوی ۶: ماسک قفل باتری خراب و زمان شارژ پیوستهٔ
     *      هر کانال به‌صورت جداگانه. */
#if MODULE_CHARGER
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         func__Charger_DeadMask());
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         func__Charger_DeadElapsedSeconds(0u));
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         func__Charger_DeadElapsedSeconds(1u));
#else
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);
#endif

    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_TLM_LIVE,
                            UINT8_T__A__Payload,
                            (uint16_t)ESPLINK_TLM_PAYLOAD_SIZE);
}

/* ==================== Frame handling / رسیدگی به فریم ==================== */

/**
 * @brief  [EN] Handle one complete, CRC-verified frame: apply a SET_PARAM
 *              (with a PARAM_REPORT reply of the applied value) or answer
 *              GET_PARAMS with PARAMS_BULK. Unknown types and wrong payload
 *              lengths are dropped silently. CAL_REFERENCE (0x03) is no longer
 *              handled - the panel stopped sending it in v1.7 and the dead
 *              handler was removed in v2 to fit the 62 K flash budget.
 *         [FA] رسیدگی به یک فریم کامل و تأییدشده با CRC: اعمال SET_PARAM (با
 *              پاسخ PARAM_REPORT حاوی مقدار اعمال‌شده) یا پاسخ GET_PARAMS با
 *              PARAMS_BULK. نوع ناشناخته و طول payload غلط بی‌صدا کنار گذاشته
 *              می‌شود. CAL_REFERENCE دیگر رسیدگی نمی‌شود: پنل از نسخهٔ ۱.۷ آن را
 *              نمی‌فرستد و هندلر مرده در نسخهٔ ۲ برای جاشدن در ۶۲ کیلوبایت فلش
 *              حذف شد.
 * @param  uint8_t__messageType [EN] Message type / نوع پیام
 * @param  uint16_t__payloadLength [EN] Payload length (v1.16 u16) / طول payload
 * @param  const uint8_t *uint8_t__payload [EN] Payload / payload
 */
/**
 * @brief  [EN] Answer one LUT stage (v1.66). Every single LUT frame is
 *              acknowledged - the panel can therefore say WHERE a push
 *              failed instead of only that it did.
 *         [FA] پاسخ به یک مرحلهٔ ارسال جدول: هر فریم ACK می‌گیرد تا پنل
 *              بتواند بگوید «کجا» شکست، نه فقط «شکست».
 */
static void func__EspLink_SendLutAck(uint8_t uint8_t__stage,
                                     uint8_t uint8_t__status,
                                     uint32_t uint32_t__crc32)
{
    uint8_t UINT8_T__A__Payload[8u];

    UINT8_T__A__Payload[0] = uint8_t__stage;
    UINT8_T__A__Payload[1] = uint8_t__status;
    /* [EN] Staged counts of THIS transaction, not the active table - the
       sender matches them against its own push (see the statics above).
       [FA] تعداد چیده‌شدهٔ همین تراکنش، نه جدول فعال - فرستنده آنها را با
       ارسال خودش مطابقت می‌دهد (توضیح متغیرهای بالا). */
    UINT8_T__A__Payload[2] = UINT8_T__G__LutStageN1;
    UINT8_T__A__Payload[3] = UINT8_T__G__LutStageN2;
    UINT8_T__A__Payload[4] = (uint8_t)(uint32_t__crc32 & 0xFFu);
    UINT8_T__A__Payload[5] = (uint8_t)((uint32_t__crc32 >> 8) & 0xFFu);
    UINT8_T__A__Payload[6] = (uint8_t)((uint32_t__crc32 >> 16) & 0xFFu);
    UINT8_T__A__Payload[7] = (uint8_t)((uint32_t__crc32 >> 24) & 0xFFu);

    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_LUT_ACK,
                            UINT8_T__A__Payload, 8u);
}

/* [EN] Keep the handshake policy as a small stateful predicate so it is
   testable without a UART or flash model. The handler supplies the facts it
   already knows: whether the magic and active-table checks passed.
   [FA] سیاست دست‌دادن را به‌صورت یک predicate کوچک و stateful نگه می‌داریم
   تا بدون مدل UART یا فلش تست‌پذیر باشد. هندلر واقعیت‌هایی را که خودش دارد
   می‌دهد: معتبر بودن مجیک و فعال بودن جدول. */
static void func__EspLink_RecordLutCommitAck(bool bool__success)
{
    BOOL__G__LutResetAuthorized = bool__success;
}

static bool func__EspLink_ConsumeLutResetAuthorization(bool bool__magicValid,
                                                        bool bool__tableActive)
{
    if ((bool__magicValid == false) ||
        (bool__tableActive == false) ||
        (BOOL__G__LutResetAuthorized == false))
    {
        return false;
    }

    BOOL__G__LutResetAuthorized = false;
    return true;
}

/**
 * @brief  [EN] The four LUT-push frames (v1.66). Kept out of HandleFrame so
 *              the hot parameter path stays as short as it was.
 *         [FA] چهار فریم ارسال جدول، جدا از HandleFrame تا مسیر داغ
 *              پارامترها به همان کوتاهی بماند.
 * @return bool [EN] true = this type was a LUT frame‎ / این نوع، فریم جدول بود
 */
static bool func__EspLink_HandleLutFrame(uint8_t uint8_t__messageType,
                                         uint16_t uint16_t__payloadLength,
                                         const uint8_t *uint8_t__payload)
{
    if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_LUT_BEGIN)
    {
        /* [EN] The latest LUT ACK is no longer the successful commit once a
           new staging transaction begins.
           [FA] با شروع تراکنش جدید، آخرین ACK دیگر ACK موفق commit نیست. */
        func__EspLink_RecordLutCommitAck(false);
        uint8_t uint8_t__status = (uint8_t)CAL_LUT_ST_COUNT;

        if (uint16_t__payloadLength == 2u)
        {
            if (func__CalLut_StageBegin((uint32_t)uint8_t__payload[0],
                                        (uint32_t)uint8_t__payload[1]) != false)
            {
                uint8_t__status = (uint8_t)CAL_LUT_ST_OK;
                UINT8_T__G__LutStageN1 = uint8_t__payload[0];
                UINT8_T__G__LutStageN2 = uint8_t__payload[1];
            }
            else
            {
                UINT8_T__G__LutStageN1 = 0u;
                UINT8_T__G__LutStageN2 = 0u;
            }
        }
        func__EspLink_SendLutAck((uint8_t)ESPLINK_LUT_ACK_STAGE_BEGIN,
                                 uint8_t__status, 0u);
        return true;
    }

    if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_LUT_CHUNK)
    {
        /* [EN] A chunk ACK supersedes a commit ACK; reset must wait for a
           fresh successful commit after this transaction step.
           [FA] ACK تکه جای ACK commit را می‌گیرد؛ بعد از این مرحله ریست باید
           منتظر commit موفق تازه بماند. */
        func__EspLink_RecordLutCommitAck(false);
        uint8_t uint8_t__status = (uint8_t)CAL_LUT_ST_MISSING;

        if (uint16_t__payloadLength >= 3u)
        {
            uint8_t uint8_t__channel = uint8_t__payload[0];
            uint8_t uint8_t__first = uint8_t__payload[1];
            uint8_t uint8_t__count = uint8_t__payload[2];

            if ((uint16_t__payloadLength ==
                 (uint16_t)(3u + (8u * (uint16_t)uint8_t__count))) &&
                (uint8_t__count > 0u))
            {
                uint8_t uint8_t__i;

                uint8_t__status = (uint8_t)CAL_LUT_ST_OK;
                for (uint8_t__i = 0u; uint8_t__i < uint8_t__count; uint8_t__i++)
                {
                    uint16_t uint16_t__offset = (uint16_t)(3u + (8u * (uint16_t)uint8_t__i));
                    uint32_t uint32_t__chainMa =
                        func__EspLink_GetU32(uint8_t__payload, uint16_t__offset);
                    uint32_t uint32_t__powerMw =
                        func__EspLink_GetU32(uint8_t__payload,
                                             (uint16_t)(uint16_t__offset + 4u));

                    if (func__CalLut_StagePoint(uint8_t__channel,
                                                (uint32_t)(uint8_t__first + uint8_t__i),
                                                uint32_t__chainMa,
                                                uint32_t__powerMw) == false)
                    {
                        uint8_t__status = (uint8_t)CAL_LUT_ST_MISSING;
                        break;
                    }
                }
            }
        }
        func__EspLink_SendLutAck((uint8_t)ESPLINK_LUT_ACK_STAGE_CHUNK,
                                 uint8_t__status, 0u);
        return true;
    }

    if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_LUT_COMMIT)
    {
        uint8_t uint8_t__status = (uint8_t)CAL_LUT_ST_NO_STAGE;
        uint32_t uint32_t__boardCrc = 0u;

        if (uint16_t__payloadLength == 4u)
        {
            uint8_t__status = func__CalLut_Commit(
                func__EspLink_GetU32(uint8_t__payload, 0u), &uint32_t__boardCrc);
        }
        func__EspLink_RecordLutCommitAck(
            uint8_t__status == (uint8_t)CAL_LUT_ST_OK);
        func__EspLink_SendLutAck((uint8_t)ESPLINK_LUT_ACK_STAGE_COMMIT,
                                 uint8_t__status, uint32_t__boardCrc);
        return true;
    }

    if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_LUT_RESET)
    {
        uint8_t uint8_t__status = (uint8_t)CAL_LUT_ST_NO_STAGE;
        bool bool__magicValid = false;
        bool bool__tableActive;

        /* [EN] Literal 'R','S','T','!' and a table that is actually active:
           a stray or replayed frame must never be able to reboot a charging
           board. The stateful predicate additionally requires the last
           LUT_ACK to have been a successful COMMIT and consumes that grant.
           [FA] مجیک متنی و وجود جدول فعال: فریم سرگردان یا تکرارشده هرگز
           نباید بردِ در حال شارژ را ریست کند. predicate علاوه بر این، ACK
           آخر را باید commit موفق بداند و مجوز را مصرف می‌کند. */
        if ((uint16_t__payloadLength == 4u) &&
            (uint8_t__payload[0] == (uint8_t)'R') &&
            (uint8_t__payload[1] == (uint8_t)'S') &&
            (uint8_t__payload[2] == (uint8_t)'T') &&
            (uint8_t__payload[3] == (uint8_t)'!'))
        {
            bool__magicValid = true;
        }
        bool__tableActive =
            (func__CalLut_Active(CAL_LUT_CHANNEL_1) != false) ||
            (func__CalLut_Active(CAL_LUT_CHANNEL_2) != false);

        if (func__EspLink_ConsumeLutResetAuthorization(bool__magicValid,
                                                        bool__tableActive) != false)
        {
            /* [EN] User bug 2026-10-07: the reboot must not outrun the
               debounced save - flush a pending record BEFORE arming, so a
               reset right after a settings edit keeps the new values.
               [FA] باگ کاربر ۲۰۲۶-۱۰-۰۷: ریست نباید از ذخیرهٔ دیبانس‌شده
               جلو بزند - رکورد معلق «قبل از» مسلح‌کردن فلاش می‌شود تا ریستِ
               بلافاصله بعد از ویرایش تنظیمات، مقادیر نو را نگه دارد. */
            func__EspLink_NvmFlushForReset();
            func__CalLut_RequestReset();
            uint8_t__status = (uint8_t)CAL_LUT_ST_OK;
        }
        func__EspLink_SendLutAck((uint8_t)ESPLINK_LUT_ACK_STAGE_RESET,
                                 uint8_t__status, func__CalLut_ActiveCrc32());
        return true;
    }

    return false;
}

static void func__EspLink_HandleFrame(uint8_t uint8_t__messageType,
                                      uint16_t uint16_t__payloadLength,
                                      const uint8_t *uint8_t__payload)
{
    uint32_t uint32_t__value;
    uint32_t uint32_t__appliedValue;

    if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_SET_PARAM)
    {
        if (uint16_t__payloadLength == 5u)
        {
            /* [EN] Valid frame: refresh the manual-mode link dead-man
               (protocol v1.2 - while manual is on, 3 s of silence makes
               the charger drop both duties and resume autonomous mode).
               [FA] فریم معتبر: مهر ددمنِ لینک مود دستی تازه می‌شود (v1.2 -
               در مود دستی، ۳ ثانیه سکوت یعنی صفرشدن هر دو duty و بازگشت
               به حالت خودکار). */
#if MODULE_CHARGER
            func__Charger_NotifyEspLinkActivity();
#endif /* MODULE_CHARGER */
            uint32_t__value = func__EspLink_GetU32(uint8_t__payload, 1u);
            if (func__EspLink_ApplyParam(uint8_t__payload[0], uint32_t__value,
                                         &uint32_t__appliedValue) != false)
            {
                func__EspLink_SendParamReport(uint8_t__payload[0],
                                              uint32_t__appliedValue);
                /* [EN] v1.14 (user order 2026-09-25: values must survive
                   power loss): a successfully applied PERSISTED parameter
                   arms the debounced flash save (transient test ids are
                   filtered inside MarkDirty).
                   [FA] v1.14 (دستور کاربر: مقادیر با قطع برق باید بمانند):
                   اعمال موفق یک پارامتر «ذخیره‌شونده»، ذخیرهٔ دیبانس‌شدهٔ
                   فلش را مسلح می‌کند (شناسه‌های تست گذرا داخل MarkDirty
                   فیلتر می‌شوند). */
                func__EspLink_NvmMarkDirty(uint8_t__payload[0]);
            }
        }
    }
    else if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_GET_PARAMS)
    {
        if (uint16_t__payloadLength == 0u)
        {
#if MODULE_CHARGER
            func__Charger_NotifyEspLinkActivity();
#endif /* MODULE_CHARGER */
            func__EspLink_SendParamsBulk();
        }
    }
    else if (func__EspLink_HandleLutFrame(uint8_t__messageType,
                                          uint16_t__payloadLength,
                                          uint8_t__payload) != false)
    {
        /* [EN] v1.66 LUT push handled (and acknowledged) above.
           [FA] ارسال جدول v1.66 بالا رسیدگی و پاسخ داده شد. */
#if MODULE_CHARGER
        func__Charger_NotifyEspLinkActivity();
#endif /* MODULE_CHARGER */
    }
    else
    {
        /* [EN] Unknown message type: ignore and resync on the next frame.
           [FA] نوع پیام ناشناخته: نادیده و همگام‌سازی روی فریم بعدی. */
    }
}

/* [EN] Return to the earliest state that can still recognize the current
   byte as the start of the next frame. Error bytes are data already consumed
   by the failing frame, but AA itself may be the next SOF0; dropping it loses
   an immediately following AA 55 and leaves the link blind until another
   frame happens to arrive.
   [FA] به ابتدایی‌ترین حالتی برگرد که هنوز می‌تواند همین بایت را شروع
   فریم بعد بداند. بایت خطا قبلاً در فریم خراب مصرف شده، اما خود AA می‌تواند
   SOF0 بعدی باشد؛ دور انداختنش جفت AA 55 بلافاصله بعد را گم می‌کند. */
static void func__EspLink_ResyncFromByte(uint8_t uint8_t__byte)
{
    ESP_LINK_PARSE_STATE_T__G__State =
        (uint8_t__byte == (uint8_t)ESPLINK_SOF_BYTE0)
            ? ESP_LINK_PARSE_WAIT_SOF1
            : ESP_LINK_PARSE_WAIT_SOF0;
}

/**
 * @brief  [EN] Feed one received byte into the parser state machine.
 *         [FA] یک بایت دریافتی را به ماشین حالت پارسر می‌دهد.
 * @param  uint8_t__byte [EN] Received byte / بایت دریافتی
 */
static void func__EspLink_ParseByte(uint8_t uint8_t__byte)
{
    switch (ESP_LINK_PARSE_STATE_T__G__State)
    {
        case ESP_LINK_PARSE_WAIT_SOF0:
            if (uint8_t__byte == (uint8_t)ESPLINK_SOF_BYTE0)
            {
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF1;
            }
            break;

        case ESP_LINK_PARSE_WAIT_SOF1:
            if (uint8_t__byte == (uint8_t)ESPLINK_SOF_BYTE1)
            {
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_VERSION;
            }
            else if (uint8_t__byte == (uint8_t)ESPLINK_SOF_BYTE0)
            {
                /* [EN] AA AA 55: the second AA is itself a fresh SOF0
                   (full-program audit 2026-09-26) - stay in WAIT_SOF1
                   instead of dropping it, so one garbage byte cannot eat
                   a whole frame.
                   [FA] AA AA 55: خود AA دوم یک SOF0 تازه است (ممیزی کل
                   برنامه) - در WAIT_SOF1 بمان تا یک بایت اضافه یک فریم
                   کامل را نخورد. */
            }
            else
            {
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
            }
            break;

        case ESP_LINK_PARSE_WAIT_VERSION:
            /* [EN] A version we do not speak means the two boards were flashed
               out of step. Parsing on would either fail on the CRC or, worse,
               accept a frame whose layout has moved. Count it and resync, so
               the condition is visible instead of looking like a dead link.
               [FA] نسخه‌ای که نمی‌شناسیم یعنی دو برد ناهماهنگ فلش شده‌اند. ادامهٔ
               تجزیه یا روی CRC می‌افتد یا بدتر، فریمی را می‌پذیرد که چیدمانش عوض
               شده. شمرده و همگام می‌شویم تا این وضع دیده شود نه اینکه شبیه لینک
               مرده به نظر برسد. */
            if (uint8_t__byte != (uint8_t)ESPLINK_PROTOCOL_VERSION)
            {
                UINT32_T__G__RxVersionMismatch++;
                func__EspLink_ResyncFromByte(uint8_t__byte);
            }
            else
            {
                UINT16_T__G__Crc = func__EspLink_Crc16((uint16_t)ESPLINK_CRC16_INIT,
                                                       uint8_t__byte);
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_TYPE;
            }
            break;

        case ESP_LINK_PARSE_WAIT_TYPE:
            UINT8_T__G__FrameType = uint8_t__byte;
            UINT16_T__G__Crc = func__EspLink_Crc16(UINT16_T__G__Crc, uint8_t__byte);
            ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_LEN_LO;
            break;

        case ESP_LINK_PARSE_WAIT_LEN_LO:
            /* [EN] v1.16: u16 little-endian length; the range check runs
               once both bytes are in.
               [FA] نسخه ۱.۱۶: طول u16 لیتل‌اندین. */
            UINT16_T__G__FrameLen = (uint16_t)uint8_t__byte;
            UINT16_T__G__Crc = func__EspLink_Crc16(UINT16_T__G__Crc, uint8_t__byte);
            ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_LEN_HI;
            break;

        case ESP_LINK_PARSE_WAIT_LEN_HI:
            UINT16_T__G__FrameLen = (uint16_t)(UINT16_T__G__FrameLen |
                ((uint16_t)((uint16_t)uint8_t__byte << 8)));
            UINT16_T__G__Crc = func__EspLink_Crc16(UINT16_T__G__Crc, uint8_t__byte);
            if (UINT16_T__G__FrameLen > (uint16_t)ESPLINK_FRAME_MAX_PAYLOAD)
            {
                /* [EN] Impossible length: resynchronize, retaining AA as
                   SOF0 when the failing high byte is itself AA.
                   [FA] طول ناممکن: همگام‌سازی دوباره؛ اگر بایت بالای خطا
                   خودش AA است، آن را به‌عنوان SOF0 نگه می‌داریم. */
                func__EspLink_ResyncFromByte(uint8_t__byte);
            }
            else
            {
                UINT16_T__G__PayloadIndex = 0u;
                if (UINT16_T__G__FrameLen == 0u)
                {
                    ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_CRC_LO;
                }
                else
                {
                    ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_PAYLOAD;
                }
            }
            break;

        case ESP_LINK_PARSE_WAIT_PAYLOAD:
            UINT8_T__G__PayloadBuffer[UINT16_T__G__PayloadIndex] = uint8_t__byte;
            UINT16_T__G__PayloadIndex = (uint16_t)(UINT16_T__G__PayloadIndex + 1u);
            UINT16_T__G__Crc = func__EspLink_Crc16(UINT16_T__G__Crc, uint8_t__byte);
            if (UINT16_T__G__PayloadIndex >= UINT16_T__G__FrameLen)
            {
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_CRC_LO;
            }
            break;

        case ESP_LINK_PARSE_WAIT_CRC_LO:
            UINT8_T__G__RxCrcLow = uint8_t__byte;
            ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_CRC_HI;
            break;

        case ESP_LINK_PARSE_WAIT_CRC_HI:
            if (((uint16_t)(((uint16_t)uint8_t__byte << 8) |
                            (uint16_t)UINT8_T__G__RxCrcLow)) == UINT16_T__G__Crc)
            {
#ifdef ESPLINK_HOST_TEST
                /* [EN] Host parser tests deliberately avoid the product
                   dispatch graph; acceptance itself is the observable. */
                UINT32_T__G__HostAcceptedFrames++;
#else
                func__EspLink_HandleFrame(UINT8_T__G__FrameType,
                                          UINT16_T__G__FrameLen,
                                          UINT8_T__G__PayloadBuffer);
#endif
            }
            else
            {
                /* [EN] Counted so a noisy link is measurable instead of just
                   feeling unreliable. [FA] شمرده می‌شود تا لینک نویزی قابل
                   اندازه‌گیری باشد نه فقط «به نظر بی‌اعتماد». */
                UINT32_T__G__RxCrcError++;
            }
            /* [EN] CRC-HI may be AA, which is already the next frame's
               SOF0. Preserve it instead of requiring a third frame to
               recover after AA 55.
               [FA] ممکن است ‎CRC-HI‎ برابر AA باشد و همان SOF0 فریم بعدی
               باشد؛ آن را نگه می‌داریم تا بعد از AA 55 به فریم بعد برسیم. */
            func__EspLink_ResyncFromByte(uint8_t__byte);
            break;

        default:
            func__EspLink_ResyncFromByte(uint8_t__byte);
            break;
    }
}

#ifdef ESPLINK_HOST_TEST
/** [EN] Host-only parser and handshake probes; never part of the firmware API. */
void func__EspLink_HostTest_Reset(void)
{
    ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
    UINT32_T__G__RxCrcError = 0u;
    UINT32_T__G__RxVersionMismatch = 0u;
    UINT32_T__G__HostAcceptedFrames = 0u;
    BOOL__G__LutResetAuthorized = false;
    UINT8_T__G__LutStageN1 = 0u;
    UINT8_T__G__LutStageN2 = 0u;
}

void func__EspLink_HostTest_FeedByte(uint8_t uint8_t__byte)
{
    func__EspLink_ParseByte(uint8_t__byte);
}

uint32_t func__EspLink_HostTest_CrcErrors(void)
{
    return UINT32_T__G__RxCrcError;
}

uint32_t func__EspLink_HostTest_VersionErrors(void)
{
    return UINT32_T__G__RxVersionMismatch;
}

uint32_t func__EspLink_HostTest_AcceptedFrames(void)
{
    return UINT32_T__G__HostAcceptedFrames;
}

void func__EspLink_HostTest_RecordCommitAck(bool bool__success)
{
    func__EspLink_RecordLutCommitAck(bool__success);
}

bool func__EspLink_HostTest_TryReset(bool bool__magicValid,
                                     bool bool__tableActive)
{
    return func__EspLink_ConsumeLutResetAuthorization(bool__magicValid,
                                                       bool__tableActive);
}

bool func__EspLink_HostTest_HandleLutFrame(uint8_t uint8_t__messageType,
                                           const uint8_t *uint8_t__payload,
                                           uint16_t uint16_t__payloadLength)
{
    return func__EspLink_HandleLutFrame(uint8_t__messageType,
                                        uint16_t__payloadLength,
                                        uint8_t__payload);
}
#endif

/* ==================== EspLink_Init ==================== */

/**
 * @brief  [EN] Bring the link up: select the UART backend (enables the
 *              USART1 interrupt and arms the interrupt-driven reception),
 *              reset the parser and power the ESP (CH_PD high). Active
 *              only while MODULE_ESP is enabled.
 *         [FA] لینک را بالا می‌آورد: انتخاب backend ی UART (فعال‌کردن وقفهٔ
 *              USART1 و مسلح‌کردن دریافت وقفه‌ای)، ریست پارسر و روشن‌کردن
 *              ESP (CH_PD high). فقط با فعال‌بودن MODULE_ESP فعال است.
 */
void func__EspLink_Init(void)
{
    func__BspUart_Init();
    ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
    BOOL__G__LutResetAuthorized = false;
    UINT8_T__G__LutStageN1 = 0u;
    UINT8_T__G__LutStageN2 = 0u;
    UINT16_T__G__TelemetrySeq = 0u;
    func__EspLink_Power(true);
}

/* ==================== EspLink_Power ==================== */

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 * @param  bool__on [EN] true=ESP on (HIGH), false=off‎ / روشن/خاموش
 */
void func__EspLink_Power(bool bool__on)
{
    func__BspGpio_Write(BSP_GPIO_ESP_CHPD, bool__on);
}

/* ==================== EspLink_Run ==================== */

/**
 * @brief  [EN] Consume every received byte (each complete command frame is
 *              applied immediately) and then send one TLM_LIVE telemetry
 *              frame. Called once per communication task period.
 *         [FA] همهٔ بایت‌های دریافتی را مصرف می‌کند (هر فریم فرمان کامل
 *              بلافاصله اعمال می‌شود) و بعد یک فریم تله‌متری TLM_LIVE
 *              می‌فرستد. یک‌بار در هر دورهٔ تسک ارتباط صدا زده می‌شود.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 * @param  fault_mask_t__faults [EN] Fault bits / بیت‌های خطا
 */
void func__EspLink_Run(const measurement_snapshot_t *measurement_snapshot_t__snap,
                       fault_mask_t fault_mask_t__faults)
{
    uint8_t uint8_t__byte;

    (void)APP_CONFIG;

    while (func__BspUart_ReadByte(&uint8_t__byte) != false)
    {
        func__EspLink_ParseByte(uint8_t__byte);
    }

    func__EspLink_SendTelemetry(measurement_snapshot_t__snap,
                                fault_mask_t__faults);

    /* [EN] Debounced flash save of the persisted parameters. A save
       stalls the WHOLE CPU ~30..50 ms (F1 single bank: SysTick jumps,
       EXTI/UART ISRs go latent, in-flight UART bytes overrun) - not just
       this task. The charger is suspended (gates at 0) around the save,
       so no switching happens inside the blind window; the stall itself
       is inherent to the part.
       [FA] ذخیرهٔ دیبانس‌شدهٔ فلش: هر ذخیره کل CPU را ~۳۰..۵۰ms نگه می‌دارد
       (تک‌بانک F1: پرش ساعت کرنل، تأخیر ISRها، افت بایت UART) - نه فقط
       همین تسک. شارژر دور ذخیره معلق می‌شود (گیت‌ها صفر) تا سوییچینگی
       داخل پنجرهٔ کور نباشد؛ خود تأخیر ذاتیِ چیپ است. */
    func__EspLink_NvmTick();

    /* [EN] v1.66: performs the reboot the panel armed AFTER a verified LUT
       push (the ACK frame has already left the UART by now).
       [FA] v1.66: ریستی که پنل بعد از ارسال تأییدشدهٔ جدول مسلح کرده را
       انجام می‌دهد (فریم ACK تا اینجا از UART خارج شده است). */
    func__CalLut_Tick();
}
