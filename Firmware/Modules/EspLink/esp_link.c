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

#include <stddef.h>

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_CHARGER
#include "charger.h"
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
    ESP_LINK_PARSE_WAIT_LEN,
    ESP_LINK_PARSE_WAIT_PAYLOAD,
    ESP_LINK_PARSE_WAIT_CHECKSUM
} esp_link_parse_state_t;

static esp_link_parse_state_t ESP_LINK_PARSE_STATE_T__G__State =
    ESP_LINK_PARSE_WAIT_SOF0;
static uint8_t UINT8_T__G__FrameType;
static uint8_t UINT8_T__G__FrameLen;
static uint8_t UINT8_T__G__PayloadIndex;
static uint8_t UINT8_T__G__PayloadBuffer[ESPLINK_FRAME_MAX_PAYLOAD];
static uint8_t UINT8_T__G__Checksum;
static uint16_t UINT16_T__G__TelemetrySeq = 0u;

/* [EN] TLM_LIVE flag bits (payload offset 2). / [FA] بیت‌های پرچم TLM_LIVE. */
#define ESPLINK_TLM_FLAG_SNAP_VALID     0x01u
#define ESPLINK_TLM_FLAG_INPUT_PRESENT  0x02u
#define ESPLINK_TLM_FLAG_MEAS_VALID     0x04u
#define ESPLINK_TLM_FLAG_CHG1_ENABLE    0x08u
#define ESPLINK_TLM_FLAG_CHG2_ENABLE    0x10u

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
static bool func__EspLink_ApplyParam(uint8_t uint8_t__paramId,
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
                (uint8_t)(uint32_t__value & 0xFFu));
            return true;
#endif

#if MODULE_CHARGER
        case ESPLINK_PARAM_CHG_EFF_UP_PERMILLE:
            *uint32_t__appliedValue =
                func__Charger_SetEfficiencyPermille(0u, uint32_t__value);
            return true;

        case ESPLINK_PARAM_CHG_EFF_DN_PERMILLE:
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
#endif

        default:
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
static bool func__EspLink_GetParam(uint8_t uint8_t__paramId,
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
        case ESPLINK_PARAM_CHG_EFF_UP_PERMILLE:
            *uint32_t__value = func__Charger_GetEfficiencyPermille(0u);
            return true;

        case ESPLINK_PARAM_CHG_EFF_DN_PERMILLE:
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
#endif

        default:
            return false;
    }
}

/* ==================== Frame transmit / ارسال فریم ==================== */

/**
 * @brief  [EN] Wrap a payload in the standard frame and transmit it.
 *         [FA] payload را در فریم استاندارد می‌پیچد و ارسال می‌کند.
 * @param  uint8_t__messageType [EN] Message type byte / بایت نوع پیام
 * @param  const uint8_t *uint8_t__payload [EN] Payload / payload
 * @param  uint8_t uint8_t__payloadLength [EN] Payload length / طول payload
 */
static void func__EspLink_SendFrame(uint8_t uint8_t__messageType,
                                    const uint8_t *uint8_t__payload,
                                    uint8_t uint8_t__payloadLength)
{
    uint8_t UINT8_T__A__Frame[ESPLINK_FRAME_HEADER_SIZE +
                              ESPLINK_FRAME_MAX_PAYLOAD +
                              ESPLINK_FRAME_CHECKSUM_SIZE];
    uint16_t uint16_t__cursor;
    uint8_t uint8_t__checksum;
    uint32_t uint32_t__i;

    if (uint8_t__payloadLength > ESPLINK_FRAME_MAX_PAYLOAD)
    {
        return;
    }

    UINT8_T__A__Frame[0] = (uint8_t)ESPLINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = (uint8_t)ESPLINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = uint8_t__messageType;
    UINT8_T__A__Frame[3] = uint8_t__payloadLength;

    for (uint32_t__i = 0u; uint32_t__i < (uint32_t)uint8_t__payloadLength; uint32_t__i++)
    {
        UINT8_T__A__Frame[ESPLINK_FRAME_HEADER_SIZE + uint32_t__i] =
            uint8_t__payload[uint32_t__i];
    }

    uint8_t__checksum = uint8_t__messageType;
    uint8_t__checksum = (uint8_t)(uint8_t__checksum ^ uint8_t__payloadLength);
    for (uint32_t__i = 0u; uint32_t__i < (uint32_t)uint8_t__payloadLength; uint32_t__i++)
    {
        uint8_t__checksum = (uint8_t)(uint8_t__checksum ^ uint8_t__payload[uint32_t__i]);
    }

    uint16_t__cursor = (uint16_t)(ESPLINK_FRAME_HEADER_SIZE + uint8_t__payloadLength);
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
                            UINT8_T__A__Payload, (uint8_t)uint16_t__cursor);
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
                            (uint8_t)ESPLINK_TLM_PAYLOAD_SIZE);
}

/* ==================== Frame handling / رسیدگی به فریم ==================== */

/**
 * @brief  [EN] Handle one complete, checksum-verified frame: apply a
 *              SET_PARAM (with a PARAM_REPORT reply of the applied value)
 *              or answer GET_PARAMS with PARAMS_BULK. Unknown types and
 *              wrong payload lengths are dropped silently.
 *         [FA] رسیدگی به یک فریم کامل و تأییدشدهٔ checksum: اعمال
 *              SET_PARAM (با پاسخ PARAM_REPORT حاوی مقدار اعمال‌شده) یا
 *              پاسخ GET_PARAMS با PARAMS_BULK. نوع ناشناخته و طول payload
 *              غلط بی‌صدا کنار گذاشته می‌شود.
 * @param  uint8_t__messageType [EN] Message type / نوع پیام
 * @param  uint8_t__payloadLength [EN] Payload length / طول payload
 * @param  const uint8_t *uint8_t__payload [EN] Payload / payload
 */
static void func__EspLink_HandleFrame(uint8_t uint8_t__messageType,
                                      uint8_t uint8_t__payloadLength,
                                      const uint8_t *uint8_t__payload)
{
    uint32_t uint32_t__value;
    uint32_t uint32_t__appliedValue;

    if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_SET_PARAM)
    {
        if (uint8_t__payloadLength == 5u)
        {
            uint32_t__value = func__EspLink_GetU32(uint8_t__payload, 1u);
            if (func__EspLink_ApplyParam(uint8_t__payload[0], uint32_t__value,
                                         &uint32_t__appliedValue) != false)
            {
                func__EspLink_SendParamReport(uint8_t__payload[0],
                                              uint32_t__appliedValue);
            }
        }
    }
    else if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_GET_PARAMS)
    {
        if (uint8_t__payloadLength == 0u)
        {
            func__EspLink_SendParamsBulk();
        }
    }
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
            else
            {
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
            }
            break;

        case ESP_LINK_PARSE_WAIT_TYPE:
            UINT8_T__G__FrameType = uint8_t__byte;
            UINT8_T__G__Checksum = uint8_t__byte;
            ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_LEN;
            break;

        case ESP_LINK_PARSE_WAIT_LEN:
            if (uint8_t__byte > (uint8_t)ESPLINK_FRAME_MAX_PAYLOAD)
            {
                /* [EN] Impossible length: resynchronize.
                   [FA] طول ناممکن: همگام‌سازی دوباره. */
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
            }
            else
            {
                UINT8_T__G__FrameLen = uint8_t__byte;
                UINT8_T__G__Checksum =
                    (uint8_t)(UINT8_T__G__Checksum ^ uint8_t__byte);
                UINT8_T__G__PayloadIndex = 0u;
                if (UINT8_T__G__FrameLen == 0u)
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
            UINT8_T__G__PayloadBuffer[UINT8_T__G__PayloadIndex] = uint8_t__byte;
            UINT8_T__G__PayloadIndex = (uint8_t)(UINT8_T__G__PayloadIndex + 1u);
            UINT8_T__G__Checksum =
                (uint8_t)(UINT8_T__G__Checksum ^ uint8_t__byte);
            if (UINT8_T__G__PayloadIndex >= UINT8_T__G__FrameLen)
            {
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_CHECKSUM;
            }
            break;

        case ESP_LINK_PARSE_WAIT_CHECKSUM:
            if (uint8_t__byte == UINT8_T__G__Checksum)
            {
                func__EspLink_HandleFrame(UINT8_T__G__FrameType,
                                          UINT8_T__G__FrameLen,
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
 * @param  app_state_t__state [EN] System state / حالت سیستم
 * @param  fault_mask_t__faults [EN] Fault bits / بیت‌های خطا
 */
void func__EspLink_Run(const measurement_snapshot_t *measurement_snapshot_t__snap,
                       app_state_t app_state_t__state,
                       fault_mask_t fault_mask_t__faults)
{
    uint8_t uint8_t__byte;

    (void)app_state_t__state;
    (void)APP_CONFIG;

    while (func__BspUart_ReadByte(&uint8_t__byte) != false)
    {
        func__EspLink_ParseByte(uint8_t__byte);
    }

    func__EspLink_SendTelemetry(measurement_snapshot_t__snap,
                                fault_mask_t__faults);
}
