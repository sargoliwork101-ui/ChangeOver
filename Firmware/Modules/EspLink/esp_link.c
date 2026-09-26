/**
 * @file    esp_link.c
 * @brief   [EN] ESP-Link engine: power control, byte-frame parser, parameter
 *              dispatch and periodic telemetry over the logical UART port
 *              (user order 2026-09-22). Pure software, static allocation
 *              only, no RTOS call inside the parser.
 *          [FA] موتور ESP-Link: کنترل تغذیه، پارسر فریم بایتی، اعمال
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

/* ==================== Parser state / وضعیت پارسر ==================== */

/**
 * @brief  [EN] Parser states of the receive state machine.
 *         [FA] وضعیت‌های پارسر ماشین حالت دریافت.
 */
typedef enum
{
    ESP_LINK_PARSE_WAIT_SOF0 = 0,
    ESP_LINK_PARSE_WAIT_SOF1,
    ESP_LINK_PARSE_WAIT_TYPE,
    ESP_LINK_PARSE_WAIT_LEN_LO,
    ESP_LINK_PARSE_WAIT_LEN_HI,
    ESP_LINK_PARSE_WAIT_PAYLOAD,
    ESP_LINK_PARSE_WAIT_CHECKSUM
} esp_link_parse_state_t;

static esp_link_parse_state_t ESP_LINK_PARSE_STATE_T__G__State =
    ESP_LINK_PARSE_WAIT_SOF0;
static uint8_t UINT8_T__G__FrameType;
static uint16_t UINT16_T__G__FrameLen;
static uint16_t UINT16_T__G__PayloadIndex;
static uint8_t UINT8_T__G__PayloadBuffer[ESPLINK_FRAME_MAX_PAYLOAD];
static uint8_t UINT8_T__G__Checksum;
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
 * @param  uint8_t__offset [EN] Byte offset / آفست بایتی
 * @return uint32_t [EN] Value / مقدار
 */
static uint32_t func__EspLink_GetU32(const uint8_t *uint8_t__payload,
                                     uint8_t uint8_t__offset)
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
 * @param  uint32_t *uint32_t__appliedValue [EN] Applied value out / اعمال‌شده
 * @return bool [EN] true when the id is known / id شناخته شد
 */
bool func__EspLink_ApplyParam(uint8_t uint8_t__paramId,
                                     uint32_t uint32_t__value,
                                     uint32_t *uint32_t__appliedValue)
{
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
#if MODULE_UI
            /* [EN] UI cadence, ids 38..76 (v1.16): range-dispatched - 39
                    case labels would drown the switch; Set re-validates.
               [FA] اعداد UI، شناسه‌های ۳۸..۷۶ (v1.16): دیسپچ بازه‌ای. */
            if ((uint8_t__paramId >= UI_ALARM_PARAM_MIN_ID) &&
                (uint8_t__paramId <= UI_ALARM_PARAM_MAX_ID))
            {
                return func__Ui_SetAlarmParam(uint8_t__paramId,
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
 * @param  uint32_t *uint32_t__value [EN] Live value out / مقدار زنده
 * @return bool [EN] true when the id is known / id شناخته شد
 */
bool func__EspLink_GetParam(uint8_t uint8_t__paramId,
                                   uint32_t *uint32_t__value)
{
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
#if MODULE_UI
            /* [EN] UI cadence live read, ids 38..76 (v1.16).
               [FA] خواندن زندهٔ اعداد UI، شناسه‌های ۳۸..۷۶. */
            if ((uint8_t__paramId >= UI_ALARM_PARAM_MIN_ID) &&
                (uint8_t__paramId <= UI_ALARM_PARAM_MAX_ID))
            {
                return func__Ui_GetAlarmParam(uint8_t__paramId,
                                              uint32_t__value);
            }
#endif
            return false;
    }
}

/* ==================== Frame transmit / ارسال فریم ==================== */

/**
 * @brief  [EN] Wrap a payload in the standard frame and transmit it.
 *         [FA] payload را در فریم استاندارد می‌پیچد و ارسال می‌کند.
 * @param  uint8_t__messageType [EN] Message type byte / بایت نوع پیام
 * @param  const uint8_t *uint8_t__payload [EN] Payload / payload
 * @param  uint16_t uint16_t__payloadLength [EN] Payload length, 0..512 (v1.16 u16 length) / طول payload
 */
static void func__EspLink_SendFrame(uint8_t uint8_t__messageType,
                                    const uint8_t *uint8_t__payload,
                                    uint16_t uint16_t__payloadLength)
{
    uint8_t UINT8_T__A__Frame[ESPLINK_FRAME_HEADER_SIZE +
                              ESPLINK_FRAME_MAX_PAYLOAD +
                              ESPLINK_FRAME_CHECKSUM_SIZE];
    uint16_t uint16_t__cursor;
    uint8_t uint8_t__checksum;
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
    UINT8_T__A__Frame[2] = uint8_t__messageType;
    UINT8_T__A__Frame[3] = uint8_t__lenLo;
    UINT8_T__A__Frame[4] = uint8_t__lenHi;

    for (uint32_t__i = 0u; uint32_t__i < (uint32_t)uint16_t__payloadLength; uint32_t__i++)
    {
        UINT8_T__A__Frame[ESPLINK_FRAME_HEADER_SIZE + uint32_t__i] =
            uint8_t__payload[uint32_t__i];
    }

    uint8_t__checksum = uint8_t__messageType;
    uint8_t__checksum = (uint8_t)(uint8_t__checksum ^ uint8_t__lenLo);
    uint8_t__checksum = (uint8_t)(uint8_t__checksum ^ uint8_t__lenHi);
    for (uint32_t__i = 0u; uint32_t__i < (uint32_t)uint16_t__payloadLength; uint32_t__i++)
    {
        uint8_t__checksum = (uint8_t)(uint8_t__checksum ^ uint8_t__payload[uint32_t__i]);
    }

    uint16_t__cursor = (uint16_t)(ESPLINK_FRAME_HEADER_SIZE + uint16_t__payloadLength);
    UINT8_T__A__Frame[uint16_t__cursor] = uint8_t__checksum;
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
 * @brief  [EN] Send one PARAMS_BULK frame with every known parameter.
 *         [FA] یک فریم PARAMS_BULK با همهٔ پارامترهای شناخته‌شده می‌فرستد.
 */
static void func__EspLink_SendParamsBulk(void)
{
    uint8_t UINT8_T__A__Payload[1u + (ESPLINK_PARAM_COUNT * 5u)];
    uint16_t uint16_t__cursor = 0u;
    uint8_t uint8_t__count = 0u;
    uint32_t uint32_t__value;

    UINT8_T__A__Payload[uint16_t__cursor] = 0u;
    uint16_t__cursor = (uint16_t)(uint16_t__cursor + 1u);

    for (uint32_t uint32_t__i = 0u; uint32_t__i < ESPLINK_PARAM_COUNT; uint32_t__i++)
    {
        if (func__EspLink_GetParam((uint8_t)uint32_t__i, &uint32_t__value) != false)
        {
            UINT8_T__A__Payload[uint16_t__cursor] = (uint8_t)uint32_t__i;
            uint16_t__cursor = (uint16_t)(uint16_t__cursor + 1u);
            UINT8_T__A__Payload[uint16_t__cursor] = (uint8_t)(uint32_t__value & 0xFFu);
            UINT8_T__A__Payload[uint16_t__cursor + 1u] =
                (uint8_t)((uint32_t__value >> 8) & 0xFFu);
            UINT8_T__A__Payload[uint16_t__cursor + 2u] =
                (uint8_t)((uint32_t__value >> 16) & 0xFFu);
            UINT8_T__A__Payload[uint16_t__cursor + 3u] =
                (uint8_t)((uint32_t__value >> 24) & 0xFFu);
            uint16_t__cursor = (uint16_t)(uint16_t__cursor + 4u);
            uint8_t__count++;
        }
    }

    UINT8_T__A__Payload[0] = uint8_t__count;
    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_PARAMS_BULK,
                            UINT8_T__A__Payload, uint16_t__cursor);
}

/**
 * @brief  [EN] Build and send one TLM_LIVE frame from the live globals.
 *         [FA] یک فریم TLM_LIVE از گلوبال‌های زنده می‌سازد و می‌فرستد.
 * @param  const measurement_snapshot_t *measurement_snapshot_t__snap [EN]
 *              Snapshot of this pass / snapshot همین پاس
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
    UINT8_T__A__Payload[uint16_t__cursor] = 0u;
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

    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_TLM_LIVE,
                            UINT8_T__A__Payload,
                            (uint16_t)ESPLINK_TLM_PAYLOAD_SIZE);
}

/* ==================== CAL_REFERENCE (v1.3) / کالیبراسیون از پنل ==================== */

/* [EN] Snapshot published by func__EspLink_Run while the RX parse loop runs
 *      (synchronously inside that call; NULL outside it, so no dangling
 *      pointer survives the task period). Only the CAL_REFERENCE handler
 *      below reads it.
 * [FA] snapshot منتشرشده از func__EspLink_Run در زمان اجرای حلقهٔ پارس RX
 *      (همزمان داخل همان فراخوانی؛ بیرون آن تهی است تا اشاره‌گر آویزان از
 *      دورهٔ تسک باقی نماند). فقط هندلر CAL_REFERENCE پایین آن را می‌خواند. */
static const measurement_snapshot_t *MEASUREMENT_SNAPSHOT_T__G__CalSnap = NULL;

#if MODULE_CHARGER
/**
 * @brief  [EN] Apply one CAL_REFERENCE (protocol v1.3, user order
 *              2026-09-24): calibrate a gain or an ETA factor from a typed
 *              multimeter reading - the firmware does the math on its own
 *              live snapshot, the panel never needs it.
 *              target 0/1 = GAIN ch1/ch2: new gain = gain x ref / i_live
 *              (the setter clamps 100..3000); that channel's ETA is reset
 *              to 0 because the old ETA absorbed the old gain - rerun
 *              target 2/3 afterwards.
 *              target 2/3 = ETA ch1/ch2: eta = ref x Vbat x 1000 /
 *              (i_live x Vin) from the live snapshot (ch1 battery =
 *              v_bat_high_mv, ch2 = v_bat_low_mv).
 *              Returns false (caller sends nothing) when the snapshot is
 *              missing/invalid, ref is outside 50..5000 mA, the live
 *              filtered current is below 50 mA, or (ETA only) Vin/Vbat are
 *              below the charger minimums.
 *         [FA] اعمال یک CAL_REFERENCE (پروتکل v1.3، دستور کاربر
 *              ۲۰۲۶-۰۹-۲۴): کالیبره‌کردن گین یا ضریب η از عدد مولتی‌متر -
 *              محاسبه را خود فریم‌ور روی snapshot زنده‌اش انجام می‌دهد و
 *              پنل به ریاضی نیاز ندارد.
 *              target 0/1 = گین کانال۱/۲: گین جدید = گین × ref ÷ جریان زنده
 *              (setter بین ۱۰۰..۳۰۰۰ گیره می‌زند)؛ η همان کانال صفر می‌شود
 *              چون η قدیمی خطای گین قدیمی را جذب کرده بود - بعدش دوباره
 *              target 2/3 بدهید.
 *              target 2/3 = η کانال۱/۲: η = ref × Vbat × ۱۰۰۰ ÷ (جریان زنده
 *              × Vin) از snapshot زنده (باتری ch1 = v_bat_high_mv و
 *              ch2 = v_bat_low_mv).
 *              false برمی‌گرداند (فراخواننده چیزی نمی‌فرستد) وقتی snapshot
 *              نیست/نامعتبر است، ref بیرون ۵۰..۵۰۰۰ mA است، جریان فیلترشدهٔ
 *              زنده زیر ۵۰ mA است، یا (فقط η) Vin/Vbat زیر حد شارژرند.
 * @param  uint8_t__target [EN] 0..3 / هدف، ۰..۳
 * @param  uint32_t__refMa [EN] Typed DMM reading, mA / عدد مولتی‌متر، mA
 * @param  uint8_t *uint8_t__paramIdOut [EN] Param id for the report / id پارامتر برای گزارش
 * @param  uint32_t *uint32_t__appliedValue [EN] Applied value out / مقدار اعمال‌شده
 * @return bool [EN] true = applied, report it / اعمال شد، گزارش بده
 */
static bool func__EspLink_ApplyCalReference(uint8_t uint8_t__target,
                                            uint32_t uint32_t__refMa,
                                            uint8_t *uint8_t__paramIdOut,
                                            uint32_t *uint32_t__appliedValue)
{
    const measurement_snapshot_t *measurement_snapshot_t__snap =
        MEASUREMENT_SNAPSHOT_T__G__CalSnap;
    uint8_t uint8_t__channelIndex;
    uint32_t uint32_t__liveMa;

    if ((measurement_snapshot_t__snap == NULL) ||
        (measurement_snapshot_t__snap->valid == false) ||
        (uint32_t__refMa < (uint32_t)ESPLINK_CAL_MIN_REF_MA) ||
        (uint32_t__refMa > (uint32_t)ESPLINK_CAL_MAX_REF_MA) ||
        (uint8_t__target > 3u))
    {
        return false;
    }

    uint8_t__channelIndex = (uint8_t)(uint8_t__target & 1u);
    uint32_t__liveMa = (uint8_t__channelIndex == 0u)
                           ? measurement_snapshot_t__snap->i_ch1_ma
                           : measurement_snapshot_t__snap->i_ch2_ma;

    if (uint32_t__liveMa < (uint32_t)ESPLINK_CAL_MIN_REF_MA)
    {
        return false;
    }

    if (uint8_t__target <= 1u)
    {
        /* [EN] GAIN: scale the live gain by ref/live; the setter clamps.
           Overflow-safe: gain <= 3000, ref <= 5000 -> product <= 1.5e7.
           [FA] گین: گین زنده به نسبت ref÷زنده؛ setter گیره می‌زند.
           امن برای سرریز: گین ≤ ۳۰۰۰ و ref ≤ ۵۰۰۰ ← حاصل‌ضرب ≤ ۱٫۵e7. */
        uint32_t uint32_t__gain =
            func__BspMeasurement_GetCurrentGainPermille(uint8_t__channelIndex);

        uint32_t__gain = (uint32_t__gain * uint32_t__refMa) / uint32_t__liveMa;

        *uint32_t__appliedValue =
            func__BspMeasurement_SetCurrentGainPermille(uint8_t__channelIndex,
                                                        uint32_t__gain);
        *uint8_t__paramIdOut = (uint8_t__channelIndex == 0u)
                                   ? ESPLINK_PARAM_CUR1_GAIN_PERMILLE
                                   : ESPLINK_PARAM_CUR2_GAIN_PERMILLE;

        /* [EN] The old ETA absorbed the old gain: reset it to identity.
           [FA] η قدیمی خطای گین قدیمی را جذب کرده بود: به همانی برگردان. */
        func__Charger_SetEfficiencyPermille(uint8_t__channelIndex, 0u);
        return true;
    }

    /* [EN] ETA: eta = ref x Vbat x 1000 / (live x Vin) on live voltages.
       Overflow-safe: ref <= 5000, Vbat <= ~15000 -> <= 7.5e7; / Vin (>=
       10000) -> <= 7500; x 1000 -> <= 7.5e6; / live (>= 50) -> clamped 999.
       [FA] η: η = ref × Vbat × ۱۰۰۰ ÷ (زنده × Vin) با ولتاژهای زنده.
       امن برای سرریز: ref ≤ ۵۰۰۰ و Vbat ≤ ~۱۵۰۰۰ ← ≤ ۷٫۵e7؛ ÷ Vin (≥۱۰۰۰۰)
       ← ≤ ۷۵۰۰؛ ×۱۰۰۰ ← ≤ ۷٫۵e6؛ ÷ زنده (≥۵۰) ← گیره در ۹۹۹. */
    {
        uint32_t uint32_t__vinMv = measurement_snapshot_t__snap->v_in_mv;
        uint32_t uint32_t__vbatMv = (uint8_t__channelIndex == 0u)
                           ? measurement_snapshot_t__snap->v_bat_high_mv
                           : measurement_snapshot_t__snap->v_bat_low_mv;
        uint32_t uint32_t__etaPermille;

        if ((uint32_t__vinMv < (uint32_t)CHG_ETA_MIN_VIN_MV) ||
            (uint32_t__vbatMv < (uint32_t)CHG_ETA_MIN_VBAT_MV))
        {
            return false;
        }

        uint32_t__etaPermille =
            (((uint32_t__refMa * uint32_t__vbatMv) / uint32_t__vinMv) * 1000u) /
            uint32_t__liveMa;

        *uint32_t__appliedValue =
            func__Charger_SetEfficiencyPermille(uint8_t__channelIndex,
                                                uint32_t__etaPermille);
        *uint8_t__paramIdOut = (uint8_t__channelIndex == 0u)
                                   ? ESPLINK_PARAM_CHG_ETA1_PERMILLE
                                   : ESPLINK_PARAM_CHG_ETA2_PERMILLE;
    }

    return true;
}
#endif /* MODULE_CHARGER */

/* ==================== Frame handling / رسیدگی به فریم ==================== */

/**
 * @brief  [EN] Handle one complete, checksum-verified frame: apply a
 *              SET_PARAM (with a PARAM_REPORT reply of the applied value),
 *              answer GET_PARAMS with PARAMS_BULK, or apply a
 *              CAL_REFERENCE (v1.3 - PARAM_REPORT replies, see
 *              func__EspLink_ApplyCalReference). Unknown types and wrong
 *              payload lengths are dropped silently.
 *         [FA] رسیدگی به یک فریم کامل و تأییدشدهٔ checksum: اعمال SET_PARAM
 *              (با پاسخ PARAM_REPORT حاوی مقدار اعمال‌شده)، پاسخ GET_PARAMS
 *              با PARAMS_BULK، یا اعمال CAL_REFERENCE (v1.3 - پاسخ‌های
 *              PARAM_REPORT، پایین را ببین). نوع ناشناخته و طول payload غلط
 *              بی‌صدا کنار گذاشته می‌شود.
 * @param  uint8_t__messageType [EN] Message type / نوع پیام
 * @param  uint16_t__payloadLength [EN] Payload length (v1.16 u16) / طول payload
 * @param  const uint8_t *uint8_t__payload [EN] Payload / payload
 */
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
            func__Charger_NotifyEspLinkActivity();
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
            func__Charger_NotifyEspLinkActivity();
            func__EspLink_SendParamsBulk();
        }
    }
#if MODULE_CHARGER
    else if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_CAL_REFERENCE)
    {
        if (uint16_t__payloadLength == 5u)
        {
            /* [EN] Valid frame: refresh the manual-mode link dead-man (see
               SET_PARAM above), then calibrate from the typed DMM value.
               GAIN targets answer with TWO PARAM_REPORT frames (the new
               gain, then the reset ETA); ETA targets with one. A rejected
               command (unstable current, bad voltages) gets NO reply.
               [FA] فریم معتبر: مهر ددمنِ لینک مود دستی (بالا را ببین) تازه
               می‌شود و بعد از عدد مولتی‌متر کالیبره می‌کنیم. target های گین
               با دو فریم PARAM_REPORT جواب می‌دهند (گین جدید و بعد η
               صفرشده)؛ target های η با یک فریم. فرمان ردشده (جریان ناپایدار،
               ولتاژ بد) هیچ پاسخی نمی‌گیرد. */
            uint8_t uint8_t__calParamId = 0u;
            uint32_t uint32_t__calApplied = 0u;

            func__Charger_NotifyEspLinkActivity();
            if (func__EspLink_ApplyCalReference(uint8_t__payload[0],
                                                func__EspLink_GetU32(uint8_t__payload, 1u),
                                                &uint8_t__calParamId,
                                                &uint32_t__calApplied) != false)
            {
                func__EspLink_SendParamReport(uint8_t__calParamId,
                                              uint32_t__calApplied);

                if (uint8_t__payload[0] <= 1u)
                {
                    /* [EN] GAIN path: also report the reset ETA (param 9/10)
                       so the panel UI returns to identity immediately.
                       [FA] مسیر گین: η صفرشده (پارامتر ۹/۱۰) هم گزارش شود
                       تا UI پنل فوراً به حالت همانی برگردد. */
                    func__EspLink_SendParamReport(
                        (uint8_t__payload[0] == 0u)
                            ? ESPLINK_PARAM_CHG_ETA1_PERMILLE
                            : ESPLINK_PARAM_CHG_ETA2_PERMILLE,
                        func__Charger_GetEfficiencyPermille(
                            (uint8_t)(uint8_t__payload[0] & 1u)));
                }
            }
        }
    }
#endif /* MODULE_CHARGER */
    else
    {
        /* [EN] Unknown message type: ignore and resync on the next frame.
           [FA] نوع پیام ناشناخته: نادیده و همگام‌سازی روی فریم بعدی. */
    }
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
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_TYPE;
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

        case ESP_LINK_PARSE_WAIT_TYPE:
            UINT8_T__G__FrameType = uint8_t__byte;
            UINT8_T__G__Checksum = uint8_t__byte;
            ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_LEN_LO;
            break;

        case ESP_LINK_PARSE_WAIT_LEN_LO:
            /* [EN] v1.16: u16 little-endian length; the range check runs
               once both bytes are in.
               [FA] نسخه ۱.۱۶: طول u16 لیتل‌اندین. */
            UINT16_T__G__FrameLen = (uint16_t)uint8_t__byte;
            UINT8_T__G__Checksum =
                (uint8_t)(UINT8_T__G__Checksum ^ uint8_t__byte);
            ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_LEN_HI;
            break;

        case ESP_LINK_PARSE_WAIT_LEN_HI:
            UINT16_T__G__FrameLen = (uint16_t)(UINT16_T__G__FrameLen |
                ((uint16_t)((uint16_t)uint8_t__byte << 8)));
            UINT8_T__G__Checksum =
                (uint8_t)(UINT8_T__G__Checksum ^ uint8_t__byte);
            if (UINT16_T__G__FrameLen > (uint16_t)ESPLINK_FRAME_MAX_PAYLOAD)
            {
                /* [EN] Impossible length: resynchronize.
                   [FA] طول ناممکن: همگام‌سازی دوباره. */
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
            }
            else
            {
                UINT16_T__G__PayloadIndex = 0u;
                if (UINT16_T__G__FrameLen == 0u)
                {
                    ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_CHECKSUM;
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
            UINT8_T__G__Checksum =
                (uint8_t)(UINT8_T__G__Checksum ^ uint8_t__byte);
            if (UINT16_T__G__PayloadIndex >= UINT16_T__G__FrameLen)
            {
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_CHECKSUM;
            }
            break;

        case ESP_LINK_PARSE_WAIT_CHECKSUM:
            if (uint8_t__byte == UINT8_T__G__Checksum)
            {
                func__EspLink_HandleFrame(UINT8_T__G__FrameType,
                                          UINT16_T__G__FrameLen,
                                          UINT8_T__G__PayloadBuffer);
            }
            ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
            break;

        default:
            ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
            break;
    }
}

/* ==================== EspLink_Init ==================== */

/**
 * @brief  [EN] Bring the link up: select the UART backend (also enables the
 *              USART1 interrupt and arms the interrupt-driven reception),
 *              reset the parser and power the ESP (CH_PD high). The link
 *              stage is active only while MODULE_ESP is enabled; the former
 *              keep-the-ESP-off safe default applied to the time before a
 *              command protocol existed (user order 2026-09-22).
 *         [FA] لینک را بالا می‌آورد: انتخاب backend ی UART (همچنین فعال‌کردن
 *              وقفهٔ USART1 و مسلح‌کردن دریافت وقفه‌ای)، ریست پارسر و
 *              روشن‌کردن ESP (CH_PD.high). این مرحله فقط با فعال‌بودن
 *              MODULE_ESP فعال است؛ پیش‌فرض امنِ قبلی (ESP خاموش) به
 *              دورهٔ قبل از وجود پروتکل فرمان مربوط بود (دستور کاربر
 *              ۲۰۲۶-۰۹-۲۲).
 */
void func__EspLink_Init(void)
{
    func__BspUart_Init();
    ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
    UINT16_T__G__TelemetrySeq = 0u;
    func__EspLink_Power(true);
}

/* ==================== EspLink_Power ==================== */

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 * @param  bool__on [EN] true=ESP on (HIGH), false=off / روشن/خاموش
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

    /* [EN] Publish the snapshot for the CAL_REFERENCE handler (the parse
       loop runs synchronously inside this call; cleared on exit so no
       dangling pointer survives the task period).
       [FA] snapshot برای هندلر CAL_REFERENCE منتشر می‌شود (حلقهٔ پارس
       همزمان داخل همین فراخوانی اجرا می‌شود؛ در خروج پاک می‌شود تا
       اشاره‌گر آویزان از دورهٔ تسک باقی نماند). */
    MEASUREMENT_SNAPSHOT_T__G__CalSnap = measurement_snapshot_t__snap;

    while (func__BspUart_ReadByte(&uint8_t__byte) != false)
    {
        func__EspLink_ParseByte(uint8_t__byte);
    }

    MEASUREMENT_SNAPSHOT_T__G__CalSnap = NULL;

    func__EspLink_SendTelemetry(measurement_snapshot_t__snap,
                                fault_mask_t__faults);

    /* [EN] v1.14: debounced flash save of the persisted parameters (a page
       erase stalls flash-fetching code ~20..40 ms once per save; this is the
       comm task, so only this task's own period stretches).
       [FA] v1.14: ذخیرهٔ دیبانس‌شدهٔ فلش پارامترهای ذخیره‌شونده (پاک‌کردن
       صفحه یک‌بار ~۲۰..۴۰ms کدِ خوانده‌شده از فلش را نگه می‌دارد؛ اینجا
       تسک ارتباط است، پس فقط دورهٔ همین تسک کش می‌آید). */
    func__EspLink_NvmTick();
}
