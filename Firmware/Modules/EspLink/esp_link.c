/**
 * @file    esp_link.c
 * @brief   [EN] ESP-Link engine: power control, byte-frame parser, parameter
 *              dispatch and periodic telemetry over the logical UART port
 *              (user order 2026-09-22). Pure software, static allocation
 *              only, no RTOS call inside the parser.
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
#if MODULE_CHANGEOVER
#include "changeover.h"
#endif
#if MODULE_DIAG
#include "diag.h"
#endif
/* ==================== Parser state ==================== */
/**
 * @brief  [EN] Parser states of the receive state machine.
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
/*
 * [EN] Link health counters. A CRC error used to be indistinguishable from a
 * quiet link; now both conditions are countable, and a version mismatch says
 * plainly that the two boards were flashed out of step.
 */
static uint32_t UINT32_T__G__RxCrcError;
static uint32_t UINT32_T__G__RxVersionMismatch;
/*
 * [EN] A reset is authorized only by the immediately preceding successful
 * LUT_COMMIT acknowledgment. A table being active is not enough: it may be
 * an old table, and a replayed LUT_RESET must not reboot a live charger.
 * BEGIN/CHUNK/failed COMMIT invalidate the authorization; accepting RESET
 * consumes it, so the same reset frame cannot be replayed.
 */
static bool BOOL__G__LutResetAuthorized = false;
/*
 * [EN] Point counts declared by the BEGIN of the current transaction,
 * echoed in EVERY LUT_ACK so the ESP can match each ACK against the push
 * it just sent. User board bug 2026-10-07: the ACK echoed
 * func__CalLut_Points() - the ACTIVE table - which is 0 on a fresh board,
 * so the ESP rejected the first push's perfectly good ACKs, retried three
 * times and reported "board did not answer the send stage" although the
 * board answered every frame.
 */
static uint8_t UINT8_T__G__LutStageN1 = 0u;
static uint8_t UINT8_T__G__LutStageN2 = 0u;
#ifdef ESPLINK_HOST_TEST
static uint32_t UINT32_T__G__HostAcceptedFrames = 0u;
/*
 * [EN] v1.110: counts physical CH_PD HIGH requests so a host test can prove
 *      the ESP is really switched on at boot (see EspLink_DriveChPd).
 */
static uint32_t UINT32_T__G__HostChPdHighWrites = 0u;
#endif
static uint16_t UINT16_T__G__TelemetrySeq = 0u;
/*
 * [EN] v1.110 BOOT CONTRACT (user symptom 2026-10-25: "board on, ESP dead,
 *      no Wi-Fi at all"). The board port leaves CH_PD LOW at boot (safe
 *      level) and only func__EspLink_Init followed by Power(true) may raise
 *      it. Both flags therefore start FALSE: they must describe the pin's
 *      real state, not a hoped-for one. With them starting TRUE the
 *      power-on branch saw "pin already up", skipped the only CH_PD write in
 *      the whole program, and the ESP8266 never received its enable - the
 *      UART kept sending telemetry into a chip that was never switched on.
 *      Proven on the host: after Init plus a mains-present power-save tick
 *      the CH_PD write counter stayed at 0.
 */
static bool     BOOL__G__EspPowerOn = false;
static bool     BOOL__G__WifiClientConnected = false;
static uint32_t UINT32_T__G__ClientActiveHoldMs = 0u;
static uint32_t UINT32_T__G__BatteryIdleElapsedMs = 0u;
static uint32_t UINT32_T__G__WifiIdleLowPowerMs = ESPLINK_WIFI_IDLE_LOW_POWER_MS;
static uint32_t UINT32_T__G__WifiCycleOnMs = ESPLINK_WIFI_CYCLE_ON_MS;
static uint32_t UINT32_T__G__WifiCycleOffMs = ESPLINK_WIFI_CYCLE_OFF_MS;
static uint32_t UINT32_T__G__WifiBatCutoffPct = ESPLINK_WIFI_BAT_CUTOFF_PCT;
static uint8_t  UINT8_T__G__BatteryPercent = 100u;
static bool     BOOL__G__LastInputPresent = true;
static bool     BOOL__G__WifiBatCutoffLatched = false;
/*
 * [EN] CH_PD grace after a logical power-off: the pin drops ESPLINK_POWER_OFF_GRACE_MS
 * later so the LAST telemetry frame (the mains cut) still reaches the panel
 * and its state log writes the row before the chip dies (user order
 * 2026-10-24: "the log must be complete"). The battery-saver policy is
 * unchanged - the ESP is still switched off, only a moment later.
 */
#define ESPLINK_POWER_OFF_GRACE_MS      400u
/*
 * [EN] Physical CH_PD level the module BELIEVES is on the pin. FALSE at boot
 *      because the board port drives the pin low before the scheduler; see
 *      the boot-contract note next to EspPowerOn. Starting it true was the
 *      v1.110 defect: the first Power(true) then skipped the GPIO write.
 */
static bool     BOOL__G__EspPinUp = false;
static bool     BOOL__G__EspPowerOffPending = false;
static uint32_t UINT32_T__G__EspPowerOffElapsedMs = 0u;
/*
 * [EN] v1.111 handshake state. HandshakeOk is the verdict the telemetry flag
 *      and the panel show; HandshakeAgeMs is how long it has been since the
 *      last accepted HELLO (advanced once per comm period, no RTOS call, so
 *      the host test drives it too). RxFrameOkCount is what the ACK reports,
 *      which is how the panel proves the board is hearing the ESP cleanly.
 */
static bool     BOOL__G__HandshakeOk = false;
static uint32_t UINT32_T__G__HandshakeAgeMs = 0u;
static uint32_t UINT32_T__G__HandshakeCount = 0u;
static uint32_t UINT32_T__G__HandshakeLostCount = 0u;
static uint32_t UINT32_T__G__RxFrameOkCount = 0u;
static uint8_t  UINT8_T__G__EspFwVersion = 0u;
static uint8_t  UINT8_T__G__EspRole = 0u;
static uint32_t UINT32_T__G__EspUptimeSec = 0u;
#define ESPLINK_TLM_FLAG_SNAP_VALID     0x01u
#define ESPLINK_TLM_FLAG_INPUT_PRESENT  0x02u
#define ESPLINK_TLM_FLAG_MEAS_VALID     0x04u
#define ESPLINK_TLM_FLAG_CHG1_ENABLE    0x08u
#define ESPLINK_TLM_FLAG_CHG2_ENABLE    0x10u
/*
 * [EN] v1.2 (user order 2026-09-23): manual test mode active on the STM.
 */
#define ESPLINK_TLM_FLAG_MANUAL_MODE    0x20u
/*
 * [EN] TLM byte 3 (formerly "reserved") since v1.43: imbalance scenario 5
 *      status bits so the panel can draw the live face without a GET.
 */
#define ESPLINK_TLM_FLAG2_IMBAL_EPISODE   0x01u
#define ESPLINK_TLM_FLAG2_IMBAL_LATCHED   0x02u
#define ESPLINK_TLM_FLAG2_IMBAL_BLOCK_OUT 0x04u
#define ESPLINK_TLM_FLAG2_IMBAL_NO_CHARGE 0x08u
/*
 * [EN] v1.86: the scenario-8 weak-battery verdict is latched on the board.
 *      The panel draws its own red-blinking lamp with this bit; without it
 *      the user could only see the face on the LEDs, never in the browser.
 */
#define ESPLINK_TLM_FLAG2_WEAK_LATCHED    0x10u
/*
 * [EN] v1.93 (user order 2026-10-23): two spare bits of the same byte, so the
 * panels can say what the board is doing RIGHT NOW without touching the
 * 128-byte payload. b5 = the battery path is cut (the load is off the
 * pack), b6 = at least one channel is in its thermal rest. Both are
 * live flags, not latches: they follow the board every frame.
 */
#define ESPLINK_TLM_FLAG2_PATH_CUT        0x20u
#define ESPLINK_TLM_FLAG2_THERMAL_REST    0x40u
/*
 * [EN] v1.111: the last spare bit of the flag byte says "the ESP and this
 *      board shook hands inside the timeout". The panel needs it to tell
 *      "telemetry is flowing" (board -> ESP only) from "the link is proven
 *      both ways". No payload layout change, so both ESP programs keep
 *      parsing the same 128-byte frame.
 */
#define ESPLINK_TLM_FLAG_LINK_OK        0x40u
/* ==================== Byte packing ==================== */
/**
 * @brief  [EN] Append one little-endian u16 at the cursor and advance it.
* @param uint8_t__buffer Frame buffer /
* @param uint16_t__cursor Write cursor /
* @param uint16_t__value Value /
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
* @param uint8_t__buffer Frame buffer /
* @param uint16_t__cursor Write cursor /
* @param uint32_t__value Value /
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
* @param uint8_t__payload Payload buffer / payload
* @param uint16_t__offset Byte offset, 0..ESPLINK_FRAME_MAX_PAYLOAD-4 /
 * @note   [EN] AUDIT 2026-10-05: the offset was uint8_t while the LUT_CHUNK
 *         loop computes 3 + 8*i against a 512-byte payload ceiling, so a
 *         frame claiming more than 31 points wrapped the offset and read the
 *         wrong bytes. Unreachable today (the panel caps at 24 points and
 *         CalLut rejects index >= 24), hardened anyway: a parser must not
 *         depend on a sender's good manners.
* @return uint32_t Value /
 */
static uint32_t func__EspLink_GetU32(const uint8_t *uint8_t__payload,
                                     uint16_t uint16_t__offset)
{
    return ((uint32_t)uint8_t__payload[uint16_t__offset] |
            ((uint32_t)uint8_t__payload[uint16_t__offset + 1u] << 8) |
            ((uint32_t)uint8_t__payload[uint16_t__offset + 2u] << 16) |
            ((uint32_t)uint8_t__payload[uint16_t__offset + 3u] << 24));
}
/* ==================== Parameter dispatch ==================== */
/**
 * @brief  [EN] Apply one parameter value through the owning module setter
 *              (every setter clamps) and report the applied value back.
 *              Returns false for an unknown id, so no report frame is sent.
* @param uint8_t__paramId Parameter id /
 * @param  uint32_t__value [EN] Raw wire value (signed fields arrive as
* @param uint32_t *uint32_t__appliedValue Applied value out /
* @return bool true when the id is known / id
 */
bool func__EspLink_ApplyParam(uint8_t uint8_t__paramId,
                                     uint32_t uint32_t__value,
                                     uint32_t *uint32_t__appliedValue)
{
    if (uint32_t__appliedValue == NULL)
    {
        return false;
    }
#if MODULE_CHANGEOVER && !defined(ESPLINK_HOST_TEST)
    /*
 * [EN] Scenario-8 reset key (slot 204, panel only): writing 0 drops the
 * weak-battery verdict now. It is a command like the scenario-5/6
 * slots, never a visible parameter and never persisted.
 */
    if (uint8_t__paramId == (uint8_t)CHANGEOVER_SLOT_WEAK_RESET_ID)
    {
        (void)func__Changeover_WeakVerdictReset();
        *uint32_t__appliedValue = 0u;
        return true;
    }
#endif
    switch (uint8_t__paramId)
    {
#if MODULE_CHANGEOVER && !defined(ESPLINK_HOST_TEST)
        /*
 * [EN] Scenario 8 (weak battery, user order 2026-10-23): the Changeover
 * module owns ids 147..157; it clamps and reports the applied value.
 */
        case ESPLINK_PARAM_WEAK_ENABLE:
        case ESPLINK_PARAM_WEAK_REST_MS:
        case ESPLINK_PARAM_WEAK_CYCLE_GUARD_MS:
        case ESPLINK_PARAM_WEAK_HEALTHY_HOLD_MS:
        case ESPLINK_PARAM_WEAK_LED_PERIOD_MS:
        case ESPLINK_PARAM_WEAK_LED_DUTY_PCT:
        case ESPLINK_PARAM_WEAK_BEEP_PERIOD_MS:
        case ESPLINK_PARAM_WEAK_BEEP_LEN_MS:
        case ESPLINK_PARAM_WEAK_BEEP_COUNT:
        case ESPLINK_PARAM_WEAK_BEEP_GAP_MS:
        case ESPLINK_PARAM_WEAK_BLOCK_OUTPUT:
        case ESPLINK_PARAM_WEAK_REST_FLOOR_MV:
            return func__Changeover_SetParam(uint8_t__paramId,
                                             uint32_t__value,
                                             uint32_t__appliedValue);
#endif
#ifndef ESPLINK_HOST_TEST
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
            /*
 * [EN] Saturate before the narrowing cast: a wrapped value (300 -> 44)
 * would be accepted silently. The setter clamps 1..MAX itself.
 */
            *uint32_t__appliedValue = (uint32_t)func__Measurement_SetFilterMedianSize(
                (uint8_t)((uint32_t__value > 255u) ? 255u : uint32_t__value));
            return true;
        case ESPLINK_PARAM_FILTER_AVERAGE_WINDOW:
            *uint32_t__appliedValue = (uint32_t)func__Measurement_SetFilterAverageWindow(
                uint32_t__value);
            return true;
#endif
#if MODULE_CHARGER
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
        /*
 * [EN] Charge profile, ids 20..26 (v1.12, user order 2026-09-25):
 * one shared profile for both channels; the charger clamps
 * the value and re-clamps every dependent, the APPLIED value
 * is reported back.
 */
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
        /*
 * [EN] Charger alarms, ids 35..37 (v1.15, user order 2026-09-26):
 * down-only safety ceilings; same clamp + applied pattern.
 */
        case ESPLINK_PARAM_CHG_ALARM_HARD_CURRENT_MA:
        case ESPLINK_PARAM_CHG_ALARM_OV_CUTOFF_MV:
        case ESPLINK_PARAM_CHG_ALARM_VALID_FLOOR_MV:
            return func__Charger_SetAlarmParam(uint8_t__paramId,
                                               uint32_t__value,
                                               uint32_t__appliedValue);
#endif
#if MODULE_FAULT
        /*
 * [EN] Fault alarms, ids 27..34 (v1.15): battery/input supervision
 * thresholds; the fault module clamps the whole set.
 */
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
        case ESPLINK_PARAM_FAULT_INPUT_HYST_LOW_MV:
        case ESPLINK_PARAM_FAULT_INPUT_HYST_HIGH_MV:
            return func__Fault_SetInputHystParam(uint8_t__paramId,
                                                 uint32_t__value,
                                                 uint32_t__appliedValue);
#endif
#endif /* ESPLINK_HOST_TEST */
        default:
#ifndef ESPLINK_HOST_TEST
#if MODULE_CHARGER
            /*
 * [EN] Two-loop CC/CV PID, ids 83..92 (v1.24): range-dispatched
 * for the same reason as the UI block - fifteen case
 * labels buy nothing; Set re-validates and clamps.
 */
            if ((uint8_t__paramId >= CHG_PID_PARAM_CURRENT_KP) &&
                (uint8_t__paramId <= CHG_PID_PARAM_VOLTAGE_DOWN_RATE))
            {
                return func__Charger_SetPidParam(uint8_t__paramId,
                                                 uint32_t__value,
                                                 uint32_t__appliedValue);
            }
            /*
 * [EN] Charger limits / backstop gains, ids 93..107 (v1.28,
 * user order): same range dispatch; the setter owns the
 * clamp window table.
 */
            if ((uint8_t__paramId >= CHG_LIMIT_PARAM_FIRST_ID) &&
                (uint8_t__paramId <= CHG_LIMIT_PARAM_LAST_ID))
            {
                return func__Charger_SetLimitParam(uint8_t__paramId,
                                                   uint32_t__value,
                                                   uint32_t__appliedValue);
            }
#endif
#if MODULE_UI
            /*
 * [EN] UI cadence, ids 38..82 (v1.16 + v1.17 append) plus the charge-side percent map 119/120 (v1.49): range-dispatched -
 * 45 case labels would drown the switch; Set re-validates.
 */
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
            /*
 * [EN] v1.87 (user order): the per-scenario enable tick of every
 * card, ids 158..164 = scenarios 1..7. Zero silences that
 * scenario's LED/beep face only; detection, latching and the
 * output gates stay untouched.
 */
            if ((uint8_t__paramId >= UI_SCENARIO_ALARM_ID_FIRST) &&
                (uint8_t__paramId <= UI_SCENARIO_ALARM_ID_LAST))
            {
                return func__Ui_SetScenarioAlarm(uint8_t__paramId,
                                                 uint32_t__value,
                                                 uint32_t__appliedValue);
            }
#endif
#if MODULE_IMBALANCE
            /*
 * [EN] Imbalance scenario 5, ids 108..118, 123..124, 132..133
 * and clean-FLOAT-cycle threshold 136 + runtime slots
 * 200..202 (NVM boot replay only; the panel never sends
 * those slots, and they are never part of a backup).
 */
            if (IMBAL_PARAM_OWNS(uint8_t__paramId) ||
                ((uint8_t__paramId >= IMBAL_SLOT_FIRST_ID) &&
                 (uint8_t__paramId <= IMBAL_SLOT_LAST_ID)))
            {
                bool bool__imbOk = func__Imbalance_SetParam(uint8_t__paramId,
                                                            uint32_t__value,
                                                            uint32_t__appliedValue);
#if MODULE_CHARGER
                if ((bool__imbOk == true) &&
                    (uint8_t__paramId == IMBAL_SLOT_LATCH_ID) &&
                    (uint32_t__value == 0u))
                {
                    func__Charger_RestartCycle();
                }
#endif
                return bool__imbOk;
            }
#endif
#if MODULE_CHARGER
            /*
 * [EN] v1.82 scenario 6 (dead battery), ids 125..131 and 134..135 +
 * runtime slot 203 (NVM boot replay of the latch mask).
 */
            if (CHG_DEAD_PARAM_OWNS(uint8_t__paramId) ||
                (uint8_t__paramId == CHG_DEAD_SLOT_MASK_ID))
            {
                return func__Charger_SetDeadParam(uint8_t__paramId,
                                                  uint32_t__value,
                                                  uint32_t__appliedValue);
            }
            /*
 * [EN] v1.102 thermal-rest tuning, ids 166..168 (user order
 * 2026-10-24): range-dispatched like the limit block; the
 * setter owns the clamp window table.
 */
            if (CHG_THERMAL_PARAM_OWNS(uint8_t__paramId))
            {
                return func__Charger_SetThermalParam(uint8_t__paramId,
                                                     uint32_t__value,
                                                     uint32_t__appliedValue);
            }
#endif
#endif /* ESPLINK_HOST_TEST */
            if (uint8_t__paramId == (uint8_t)ESPLINK_PARAM_WIFI_IDLE_LOW_POWER_MS)
            {
                uint32_t uint32_t__clamped = uint32_t__value;
                if (uint32_t__clamped > (uint32_t)ESPLINK_WIFI_IDLE_LOW_POWER_MAX_MS)
                {
                    uint32_t__clamped = (uint32_t)ESPLINK_WIFI_IDLE_LOW_POWER_MAX_MS;
                }
                UINT32_T__G__WifiIdleLowPowerMs = uint32_t__clamped;
                *uint32_t__appliedValue = uint32_t__clamped;
                return true;
            }
            if (uint8_t__paramId == (uint8_t)ESPLINK_PARAM_WIFI_CYCLE_ON_MS)
            {
                uint32_t uint32_t__clamped = uint32_t__value;
                if (uint32_t__clamped < (uint32_t)ESPLINK_WIFI_CYCLE_ON_MIN_MS)
                {
                    uint32_t__clamped = (uint32_t)ESPLINK_WIFI_CYCLE_ON_MIN_MS;
                }
                else if (uint32_t__clamped > (uint32_t)ESPLINK_WIFI_CYCLE_ON_MAX_MS)
                {
                    uint32_t__clamped = (uint32_t)ESPLINK_WIFI_CYCLE_ON_MAX_MS;
                }
                UINT32_T__G__WifiCycleOnMs = uint32_t__clamped;
                *uint32_t__appliedValue = uint32_t__clamped;
                return true;
            }
            if (uint8_t__paramId == (uint8_t)ESPLINK_PARAM_WIFI_CYCLE_OFF_MS)
            {
                uint32_t uint32_t__clamped = uint32_t__value;
                if (uint32_t__clamped > (uint32_t)ESPLINK_WIFI_CYCLE_OFF_MAX_MS)
                {
                    uint32_t__clamped = (uint32_t)ESPLINK_WIFI_CYCLE_OFF_MAX_MS;
                }
                UINT32_T__G__WifiCycleOffMs = uint32_t__clamped;
                *uint32_t__appliedValue = uint32_t__clamped;
                return true;
            }
            if (uint8_t__paramId == (uint8_t)ESPLINK_PARAM_WIFI_BAT_CUTOFF_PCT)
            {
                uint32_t uint32_t__clamped = uint32_t__value;
                if (uint32_t__clamped > (uint32_t)ESPLINK_WIFI_BAT_CUTOFF_MAX_PCT)
                {
                    uint32_t__clamped = (uint32_t)ESPLINK_WIFI_BAT_CUTOFF_MAX_PCT;
                }
                UINT32_T__G__WifiBatCutoffPct = uint32_t__clamped;
                *uint32_t__appliedValue = uint32_t__clamped;
                return true;
            }
            return false;
    }
}
/**
 * @brief  [EN] Read the live value of one parameter for the report frames.
* @param uint8_t__paramId Parameter id /
* @param uint32_t *uint32_t__value Live value out /
* @return bool true when the id is known / id
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
#if MODULE_CHANGEOVER && !defined(ESPLINK_HOST_TEST)
        /*
 * [EN] Scenario 8 live read, ids 147..157 (the NVM snapshot and the
 * report frames both read through here).
 */
        case ESPLINK_PARAM_WEAK_ENABLE:
        case ESPLINK_PARAM_WEAK_REST_MS:
        case ESPLINK_PARAM_WEAK_CYCLE_GUARD_MS:
        case ESPLINK_PARAM_WEAK_HEALTHY_HOLD_MS:
        case ESPLINK_PARAM_WEAK_LED_PERIOD_MS:
        case ESPLINK_PARAM_WEAK_LED_DUTY_PCT:
        case ESPLINK_PARAM_WEAK_BEEP_PERIOD_MS:
        case ESPLINK_PARAM_WEAK_BEEP_LEN_MS:
        case ESPLINK_PARAM_WEAK_BEEP_COUNT:
        case ESPLINK_PARAM_WEAK_BEEP_GAP_MS:
        case ESPLINK_PARAM_WEAK_BLOCK_OUTPUT:
        case ESPLINK_PARAM_WEAK_REST_FLOOR_MV:
            return func__Changeover_GetParam(uint8_t__paramId, uint32_t__value);
#endif
#ifndef ESPLINK_HOST_TEST
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
        /*
 * [EN] Charge profile live read, ids 20..26 (v1.12).
 */
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
        case ESPLINK_PARAM_FAULT_INPUT_HYST_LOW_MV:
        case ESPLINK_PARAM_FAULT_INPUT_HYST_HIGH_MV:
            return func__Fault_GetInputHystParam(uint8_t__paramId,
                                                 uint32_t__value);
#endif
#endif /* ESPLINK_HOST_TEST */
        default:
#ifndef ESPLINK_HOST_TEST
#if MODULE_CHARGER
            /*
 * [EN] Two-loop CC/CV PID live read, ids 83..92 (v1.23).
 */
            if ((uint8_t__paramId >= CHG_PID_PARAM_CURRENT_KP) &&
                (uint8_t__paramId <= CHG_PID_PARAM_VOLTAGE_DOWN_RATE))
            {
                return func__Charger_GetPidParam(uint8_t__paramId,
                                                 uint32_t__value);
            }
            /*
 * [EN] Charger limits / backstop gains live read, ids 93..107.
 */
            if ((uint8_t__paramId >= CHG_LIMIT_PARAM_FIRST_ID) &&
                (uint8_t__paramId <= CHG_LIMIT_PARAM_LAST_ID))
            {
                return func__Charger_GetLimitParam(uint8_t__paramId,
                                                   uint32_t__value);
            }
#endif
#if MODULE_UI
            /*
 * [EN] UI cadence live read, ids 38..82 plus the charge map 119/120 (v1.49).
 */
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
            /*
 * [EN] v1.87: live read of the per-scenario LED/beep ticks 158..164
 * so the panel's checkboxes come back after a reboot.
 */
            if ((uint8_t__paramId >= UI_SCENARIO_ALARM_ID_FIRST) &&
                (uint8_t__paramId <= UI_SCENARIO_ALARM_ID_LAST))
            {
                return func__Ui_GetScenarioAlarm(uint8_t__paramId,
                                                 uint32_t__value);
            }
#endif
#if MODULE_IMBALANCE
            /*
 * [EN] Imbalance live read, ids 108..118 + slots 200..202
 * (the NVM save snapshots its persisted set through here).
 */
            if (IMBAL_PARAM_OWNS(uint8_t__paramId) ||
                ((uint8_t__paramId >= IMBAL_SLOT_FIRST_ID) &&
                 (uint8_t__paramId <= IMBAL_SLOT_LAST_ID)))
            {
                return func__Imbalance_GetParam(uint8_t__paramId,
                                                uint32_t__value);
            }
#endif
#if MODULE_CHARGER
            /*
 * [EN] v1.82 scenario 6 live read, ids 125..131 and 134..135 + slot 203.
 */
            if (CHG_DEAD_PARAM_OWNS(uint8_t__paramId) ||
                (uint8_t__paramId == CHG_DEAD_SLOT_MASK_ID))
            {
                return func__Charger_GetDeadParam(uint8_t__paramId,
                                                  uint32_t__value);
            }
            /*
 * [EN] v1.102 thermal-rest live read, ids 166..168.
 */
            if (CHG_THERMAL_PARAM_OWNS(uint8_t__paramId))
            {
                return func__Charger_GetThermalParam(uint8_t__paramId,
                                                     uint32_t__value);
            }
#endif
#endif /* ESPLINK_HOST_TEST */
            if (uint8_t__paramId == (uint8_t)ESPLINK_PARAM_WIFI_IDLE_LOW_POWER_MS)
            {
                *uint32_t__value = UINT32_T__G__WifiIdleLowPowerMs;
                return true;
            }
            if (uint8_t__paramId == (uint8_t)ESPLINK_PARAM_WIFI_CYCLE_ON_MS)
            {
                *uint32_t__value = UINT32_T__G__WifiCycleOnMs;
                return true;
            }
            if (uint8_t__paramId == (uint8_t)ESPLINK_PARAM_WIFI_CYCLE_OFF_MS)
            {
                *uint32_t__value = UINT32_T__G__WifiCycleOffMs;
                return true;
            }
            if (uint8_t__paramId == (uint8_t)ESPLINK_PARAM_WIFI_BAT_CUTOFF_PCT)
            {
                *uint32_t__value = UINT32_T__G__WifiBatCutoffPct;
                return true;
            }
            return false;
    }
}
/* ==================== Frame transmit ==================== */
/* ==================== CRC-16 ==================== */
/**
 * @brief  [EN] CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no
 *              final xor). Bitwise on purpose: a 256-entry table would cost
 *              512 bytes of flash on a part that does not have it to spare, and
 *              these frames are at most ~470 bytes at 10 Hz, so the loop is far
 *              cheaper than the table.
* @param uint16_t__crc Running value /
* @param uint8_t__byte Next byte /
* @return uint16_t Updated CRC / CRC
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
* @param uint8_t__messageType Message type byte /
 * @param  const uint8_t *uint8_t__payload [EN] Payload / payload
* @param uint16_t uint16_t__payloadLength Payload length, 0..512 (v1.16 u16 length) / payload
 */
static void func__EspLink_SendFrame(uint8_t uint8_t__messageType,
                                    const uint8_t *uint8_t__payload,
                                    uint16_t uint16_t__payloadLength)
{
    /*
 * [EN] STATIC by necessity, not style (v1.67 RAM audit, tools/measure_ram.py):
 * this 520-byte buffer on the stack made the worst-case comm chain
 * TaskComm -> EspLink_Run -> SendLutAck -> SendFrame -> BspUart_Write ->
 * PumpTx -> HAL_UART_Transmit_DMA -> HAL_DMA_Start_IT reach 932 of the
 * 1024-byte comm stack (91%) - below the usual 70% ceiling only by luck,
 * and one nested interrupt away from a silent overflow into the next
 * task's stack. Only the comm task ever sends a frame and the function is
 * non-reentrant (it never calls anything that calls back into it), so a
 * static buffer is race-free here - the same rationale as
 * SendParamsBulk's payload and the NVM save scratch. Cost: 520 bytes of
 * .bss out of the 3.5 KiB that were free; gain: the chain drops to about
 * 410 bytes (40%).
 */
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
    /*
 * [EN] v1.16: u16 little-endian length (len_lo + len_hi).
 */
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
    /*
 * [EN] The CRC covers version, type, both length bytes and the payload -
 */
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
 * @brief  [EN] Send one PARAM_NAK frame: a SET_PARAM that was not applied.
* @param uint8_t__paramId Parameter id (0xFF when unknown) / (0xFF )
 * @param  uint8_t__reason  [EN] ESPLINK_NAK_* / [FA] ESPLINK_NAK_*
 */
static void func__EspLink_SendParamNak(uint8_t uint8_t__paramId,
                                       uint8_t uint8_t__reason)
{
    uint8_t UINT8_T__A__Payload[2u];
    UINT8_T__A__Payload[0] = uint8_t__paramId;
    UINT8_T__A__Payload[1] = uint8_t__reason;
    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_PARAM_NAK,
                            UINT8_T__A__Payload, 2u);
}
/**
 * @brief  [EN] Send one PARAM_REPORT frame (id + applied value).
* @param uint8_t__paramId Parameter id /
* @param uint32_t__appliedValue Applied value /
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
 */
/*
 * [EN] v1.28: the single-frame reply hit its ceiling. One frame holds
 *      1 + N*5 bytes against ESPLINK_FRAME_MAX_PAYLOAD, i.e. at most 102
 *      parameters, and the user-ordered limits block took the count to 108.
 *      Rather than raise the payload ceiling - which costs RAM in a static
 *      buffer on both sides of a 20 KiB part - the reply is now CHUNKED.
 *      This works without touching the wire format because every item
 *      carries its own id and the ESP parser is count-driven: it already
 *      merges whatever ids arrive, in any grouping.
 *      The chunk size is DERIVED from the payload ceiling, so it can never
 *      disagree with the buffer it has to fit.
 */
/*
 * [EN] The link is the ONLY way the panel can reach the charger limit block,
 * and this file is the only one that sees both names - so this is where
 * the check belongs. A parameter count that stops short of the last limit
 * id compiles, runs, and simply makes the tail of the block unreachable:
 * the parameter is "settable" in the header and invisible in the panel.
 * That is precisely the failure the user ordered removed, so it is a
 * build error, not a comment. Mutation testing found this gap: lowering
 * ESPLINK_PARAM_COUNT by one passed every check that existed.
 */
/*
 * [EN] Full-program audit 2026-10-05: the pair below names charger macros,
 * so it only exists when the charger header was actually included (the
 * #include above is itself inside #if MODULE_CHARGER). Without the guard,
 * MODULE_CHARGER 0 turned these two asserts into undeclared-identifier
 * errors instead of simply dropping a charger-only contract.
 */
#if MODULE_CHARGER
_Static_assert(ESPLINK_PARAM_COUNT > CHG_LIMIT_PARAM_LAST_ID,
               "ESPLINK_PARAM_COUNT must cover the whole charger limit block");
_Static_assert(CHG_LIMIT_PARAM_FIRST_ID + CHG_LIMIT_COUNT - 1u
               == CHG_LIMIT_PARAM_LAST_ID,
               "the charger limit block has a hole in its wire-id range");
_Static_assert(CHG_THERMAL_PARAM_LAST_ID < ESPLINK_PARAM_COUNT,
               "ESPLINK_PARAM_COUNT must cover the whole thermal block");
_Static_assert(CHG_THERMAL_PARAM_FIRST_ID + CHG_THERMAL_PARAM_COUNT - 1u
               == CHG_THERMAL_PARAM_LAST_ID,
               "the thermal block has a hole in its wire-id range");
#endif /* MODULE_CHARGER */
#if MODULE_FAULT
_Static_assert(ESPLINK_PARAM_FAULT_INPUT_HYST_LOW_MV == FAULT_ALARM_PARAM_INPUT_HYST_LOW_MV,
               "input hysteresis low id must match the Fault owner");
_Static_assert(ESPLINK_PARAM_FAULT_INPUT_HYST_HIGH_MV == FAULT_ALARM_PARAM_INPUT_HYST_HIGH_MV,
               "input hysteresis high id must match the Fault owner");
_Static_assert(ESPLINK_PARAM_FAULT_INPUT_HYST_HIGH_MV < ESPLINK_PARAM_COUNT,
               "ESPLINK_PARAM_COUNT must cover the shared input hysteresis pair");
#endif /* MODULE_FAULT */
#define ESPLINK_BULK_ITEM_SIZE   5u
#define ESPLINK_BULK_MAX_ITEMS   \
    ((ESPLINK_FRAME_MAX_PAYLOAD - 1u) / ESPLINK_BULK_ITEM_SIZE)
_Static_assert(ESPLINK_BULK_MAX_ITEMS >= 1u,
               "a bulk frame must carry at least one parameter");
_Static_assert(ESPLINK_BULK_MAX_ITEMS <= 255u,
               "the bulk count byte cannot describe more than 255 items");
/* ==================== EspLink_SendParamsBulk ==================== */
static void func__EspLink_SendParamsBulk(void)
{
    /*
 * [EN] STATIC by necessity, not style (v1.16 audit E1): on the 1 KiB
 * comm stack next to SendFrame's own frame buffer this would leave only
 * dozens of bytes of margin. Single task (comm), non-reentrant, so
 * static is race-free here (same rationale as the NVM save scratch).
 * Sized to ONE chunk now rather than to the whole parameter list, which
 * is why growing the parameter count no longer grows this buffer.
 */
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
        /*
 * [EN] Chunk full: ship it and start the next one.
 */
        if (uint8_t__count >= ESPLINK_BULK_MAX_ITEMS)
        {
            UINT8_T__A__Payload[0] = uint8_t__count;
            func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_PARAMS_BULK,
                                    UINT8_T__A__Payload, uint16_t__cursor);
            uint16_t__cursor = 1u;
            uint8_t__count = 0u;
        }
    }
    /*
 * [EN] Trailing partial chunk. Sent even when empty is NOT wanted - an
 * empty bulk frame would tell the panel "zero parameters known".
 */
    if (uint8_t__count > 0u)
    {
        UINT8_T__A__Payload[0] = uint8_t__count;
        func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_PARAMS_BULK,
                                UINT8_T__A__Payload, uint16_t__cursor);
    }
}
/* ==================== Handshake / (v1.111) ==================== */
/*
 * [EN] Board side flags reported in HELLO_ACK: what this firmware actually has
 *      compiled in, so the panel can show what it is talking to instead of
 *      guessing from a behaviour difference.
 */
static uint8_t func__EspLink_BoardFlags(void)
{
    uint8_t uint8_t__flags = 0u;
    uint8_t__flags |= (uint8_t)ESPLINK_BOARD_FLAG_NVM_LOADED;
#if MODULE_CHARGER
    uint8_t__flags |= (uint8_t)ESPLINK_BOARD_FLAG_CHARGER;
#endif
#if MODULE_IMBALANCE
    uint8_t__flags |= (uint8_t)ESPLINK_BOARD_FLAG_IMBALANCE;
#endif
#if MODULE_DIAG
    uint8_t__flags |= (uint8_t)ESPLINK_BOARD_FLAG_DIAG;
#endif
    return uint8_t__flags;
}
/*
 * [EN] One counter narrowed to u16 for the wire. The ACK has a fixed size, and
 *      a wrapped counter is honest enough for a health readout - the panel only
 *      asks "is it zero or not".
 */
static uint16_t func__EspLink_ClampCounter(uint32_t uint32_t__value)
{
    if (uint32_t__value > 0xFFFFu)
    {
        return (uint16_t)0xFFFFu;
    }
    return (uint16_t)uint32_t__value;
}
/**
 * @brief  [EN] Answer one HELLO with HELLO_ACK: protocol version, board
 *              firmware version, compiled-in flags, board uptime, the board's
 *              own receive health (accepted frames, CRC errors, version
 *              mismatches), the parameter count and telemetry period, and an
 *              echo of the ESP uptime so the sender can match the answer to
 *              its own request.
 * @param  uint32_t__espUptimeSec [EN] ESP uptime echoed back, seconds /
 */
static void func__EspLink_SendHelloAck(uint32_t uint32_t__espUptimeSec)
{
    uint8_t UINT8_T__A__Payload[ESPLINK_HELLO_ACK_LEN];
    uint16_t uint16_t__cursor = 0u;
    uint32_t uint32_t__boardUptimeSec = 0u;
#if MODULE_DIAG && !defined(ESPLINK_HOST_TEST)
    {
        const diag_record_t *diag_record_t__record = func__Diag_GetRecord();
        if (diag_record_t__record != NULL)
        {
            uint32_t__boardUptimeSec = diag_record_t__record->uint32_t__uptimeSec;
        }
    }
#endif
    UINT8_T__A__Payload[uint16_t__cursor] = (uint8_t)ESPLINK_PROTOCOL_VERSION;
    uint16_t__cursor = (uint16_t)(uint16_t__cursor + 1u);
    UINT8_T__A__Payload[uint16_t__cursor] = (uint8_t)ESPLINK_BOARD_FW_VERSION;
    uint16_t__cursor = (uint16_t)(uint16_t__cursor + 1u);
    UINT8_T__A__Payload[uint16_t__cursor] = func__EspLink_BoardFlags();
    uint16_t__cursor = (uint16_t)(uint16_t__cursor + 1u);
    UINT8_T__A__Payload[uint16_t__cursor] = 0u;
    uint16_t__cursor = (uint16_t)(uint16_t__cursor + 1u);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, uint32_t__boardUptimeSec);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, UINT32_T__G__RxFrameOkCount);
    func__EspLink_PutU16(UINT8_T__A__Payload, &uint16_t__cursor,
                         func__EspLink_ClampCounter(UINT32_T__G__RxCrcError));
    func__EspLink_PutU16(UINT8_T__A__Payload, &uint16_t__cursor,
                         func__EspLink_ClampCounter(UINT32_T__G__RxVersionMismatch));
    func__EspLink_PutU16(UINT8_T__A__Payload, &uint16_t__cursor,
                         (uint16_t)ESPLINK_PARAM_COUNT);
    func__EspLink_PutU16(UINT8_T__A__Payload, &uint16_t__cursor,
                         (uint16_t)APP_CONFIG.comm_period_ms);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, uint32_t__espUptimeSec);
    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_HELLO_ACK,
                            UINT8_T__A__Payload, (uint16_t)ESPLINK_HELLO_ACK_LEN);
}
/**
 * @brief  [EN] Take one HELLO from the ESP: remember who is on the other end,
 *              restart the timeout, count it and answer with HELLO_ACK.
 *              A short or foreign-protocol HELLO is refused and changes
 *              nothing, so a corrupted frame can never fake a link.
 * @param  uint8_t__payload [EN] Payload / payload
* @param uint16_t__payloadLength Length /
* @return bool true when accepted / true
 */
static bool func__EspLink_HandleHello(const uint8_t *uint8_t__payload,
                                      uint16_t uint16_t__payloadLength)
{
    uint32_t uint32_t__espUptimeSec;
    if (uint16_t__payloadLength < (uint16_t)ESPLINK_HELLO_LEN)
    {
        return false;
    }
    if (uint8_t__payload[0] != (uint8_t)ESPLINK_PROTOCOL_VERSION)
    {
        return false;
    }
    UINT8_T__G__EspFwVersion = uint8_t__payload[1];
    UINT8_T__G__EspRole = uint8_t__payload[2];
    uint32_t__espUptimeSec = func__EspLink_GetU32(uint8_t__payload, 4u);
    UINT32_T__G__EspUptimeSec = uint32_t__espUptimeSec;
    BOOL__G__HandshakeOk = true;
    UINT32_T__G__HandshakeAgeMs = 0u;
    UINT32_T__G__HandshakeCount++;
    func__EspLink_SendHelloAck(uint32_t__espUptimeSec);
    return true;
}
/**
 * @brief  [EN] Advance the handshake timeout by one comm period; a silent ESP
 *              loses the verdict and counts one loss (edge only, so the
 *              counter measures outages, not periods).
* @param uint32_t__stepMs Elapsed step, ms / (ms)
 */
static void func__EspLink_HandshakeTick(uint32_t uint32_t__stepMs)
{
    if (BOOL__G__HandshakeOk == false)
    {
        return;
    }
    if ((UINT32_MAX - UINT32_T__G__HandshakeAgeMs) > uint32_t__stepMs)
    {
        UINT32_T__G__HandshakeAgeMs += uint32_t__stepMs;
    }
    else
    {
        UINT32_T__G__HandshakeAgeMs = UINT32_MAX;
    }
    if (UINT32_T__G__HandshakeAgeMs >= (uint32_t)ESPLINK_HANDSHAKE_TIMEOUT_MS)
    {
        BOOL__G__HandshakeOk = false;
        UINT32_T__G__HandshakeAgeMs = (uint32_t)ESPLINK_HANDSHAKE_TIMEOUT_MS;
        UINT32_T__G__HandshakeLostCount++;
    }
}
/**
 * @brief  [EN] Build and send one TLM_LIVE frame from the live globals.
 * @param  const measurement_snapshot_t *measurement_snapshot_t__snap [EN]
* @param fault_mask_t fault_mask_t__faults Fault bits /
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
    /*
 * [EN] v1.111: the link bit rides the flag byte, so a panel can tell
     *      "frames are arriving" from "the board confirms the handshake".
 */
    if (BOOL__G__HandshakeOk != false)
    {
        uint8_t__flags = (uint8_t)(uint8_t__flags | ESPLINK_TLM_FLAG_LINK_OK);
    }
    func__EspLink_PutU16(UINT8_T__A__Payload, &uint16_t__cursor,
                         UINT16_T__G__TelemetrySeq);
    UINT16_T__G__TelemetrySeq = (uint16_t)(UINT16_T__G__TelemetrySeq + 1u);
    UINT8_T__A__Payload[uint16_t__cursor] = uint8_t__flags;
    uint16_t__cursor = (uint16_t)(uint16_t__cursor + 1u);
    /*
 * [EN] v1.43: the old reserved byte now carries the imbalance scenario 5
     *      status bits (episode / latched / output-blocked / charge-halted).
 */
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
#if MODULE_CHANGEOVER && !defined(ESPLINK_HOST_TEST)
        /*
 * [EN] Scenario 8: one bit is enough for the panel - the verdict is a
         *      latch, and its cause/route live in the scenario-8 card.
 */
        if (func__Changeover_WeakVerdictActive() != false)
        {
            uint8_t__imbalanceFlags |= ESPLINK_TLM_FLAG2_WEAK_LATCHED;
        }
        if (func__Changeover_PathBlocked() != false)
        {
            uint8_t__imbalanceFlags |= ESPLINK_TLM_FLAG2_PATH_CUT;
        }
#endif
#if MODULE_CHARGER
        /*
 * [EN] v1.93: thermal rest is a visible state, not a hidden timer.
 */
        if (func__Charger_IsAnyChannelInThermalRest() != false)
        {
            uint8_t__imbalanceFlags |= ESPLINK_TLM_FLAG2_THERMAL_REST;
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
    /*
 * [EN] v1.25 CALIBRATION GROUND TRUTH: raw ADC counts, before the divider
 * maths, before the runtime offsets, before any bench compensation. Counts
 * are the only numbers on this board that no coefficient can distort, so a
 * sweep that logs these beside a DMM lets every scale be rebuilt from
 * first principles rather than tuned on top of what the firmware already
 * believes. VDDA rides along so the shared reference can be checked too.
 */
    /*
 * [EN] Full-program audit 2026-10-05: this block read the Measurement
 * globals unguarded while every other Measurement block in this frame
 * is wrapped, so MODULE_MEASUREMENT 0 broke the build. Guarded now with
 * the same zero-fill else branch, which keeps the payload layout and
 * length identical in both configurations.
 */
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
    /*
 * [EN] v1.43 imbalance live block (appended at the very end so every
     *      earlier index is untouched): |vhigh-vlow| mV, episode count,
     *      charge cycles since latch. Snap-validity is conveyed by the
     *      flags byte as usual.
 */
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
    /*
 * [EN] v1.72 scenario 6 live block (appended after the imbalance block,
     *      again at the very end so no earlier index moves): the latched
     *      dead-battery channel mask (bit0 = ch1, bit1 = ch2) and the
     *      continuous charge time of EACH channel in seconds, which the panel
     *      draws as that battery's progress toward the 24 h verdict.
     *      v1.76 (user order: "this timer must be counted separately for each
     *      battery"): the second channel used to be hidden behind a max().
 */
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
#if MODULE_DIAG
/* ==================== Diag Data send ==================== */
/**
 * @brief  [EN] Send one DIAG_DATA chunk. Chunk 0 is the summary (39 bytes
 *              with extended per-channel counts), chunks 1-2 are errors
 *              (5 each, 32 bytes per error), chunks 3-4 are power events
 *              (10 each, 12 bytes per event). Compatible with
 *              ESP_USER simple_link.h parser.
 */
static void func__EspLink_SendDiagChunk0(void)
{
    uint8_t UINT8_T__A__Payload[64];
    uint16_t uint16_t__cursor = 0u;
    int32_t int32_t__savedLock = func__Diag_LockRecord();
    const diag_record_t *diag_record_t__rec = func__Diag_GetRecord();
    uint32_t uint32_t__boardTime = func__Diag_GetBoardTime();
    if (uint32_t__boardTime == 0u)
    {
        uint32_t__boardTime = diag_record_t__rec->uint32_t__uptimeSec;
    }
    UINT8_T__A__Payload[0] = 0u;
    uint16_t__cursor = 1u;
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         diag_record_t__rec->uint32_t__totalChargeSec);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         diag_record_t__rec->uint32_t__chargeCount);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         diag_record_t__rec->uint32_t__uptimeSec);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         diag_record_t__rec->uint32_t__resetCount);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         uint32_t__boardTime);
    UINT8_T__A__Payload[uint16_t__cursor] = diag_record_t__rec->uint8_t__errorCount;
    uint16_t__cursor++;
    UINT8_T__A__Payload[uint16_t__cursor] = diag_record_t__rec->uint8_t__powerEventCount;
    uint16_t__cursor++;
    /* extended */
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         diag_record_t__rec->uint32_t__chargeCountCh1);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         diag_record_t__rec->uint32_t__chargeCountCh2);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         diag_record_t__rec->uint32_t__chargeSecCh1);
    func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                         diag_record_t__rec->uint32_t__chargeSecCh2);
    func__Diag_UnlockRecord(int32_t__savedLock);
    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_DIAG_DATA,
                            UINT8_T__A__Payload, uint16_t__cursor);
}
static void func__EspLink_SendDiagErrors(uint8_t uint8_t__chunkId,
                                         uint8_t uint8_t__start,
                                         uint8_t uint8_t__count)
{
    /* [EN] Each error = 8*u32 =32 bytes, payload = 3 + count*32 <= 163 for 5 */
    uint8_t UINT8_T__A__Payload[3u + (5u * 32u)];
    uint16_t uint16_t__cursor = 0u;
    int32_t int32_t__savedLock;
    const diag_record_t *diag_record_t__rec;
    uint8_t uint8_t__i;
    if (uint8_t__count > 5u) uint8_t__count = 5u;
    if (uint8_t__start >= DIAG_MAX_ERRORS) return;
    if ((uint8_t__start + uint8_t__count) > DIAG_MAX_ERRORS)
    {
        uint8_t__count = (uint8_t)(DIAG_MAX_ERRORS - uint8_t__start);
    }
    int32_t__savedLock = func__Diag_LockRecord();
    diag_record_t__rec = func__Diag_GetRecord();
    UINT8_T__A__Payload[0] = uint8_t__chunkId;
    UINT8_T__A__Payload[1] = uint8_t__start;
    UINT8_T__A__Payload[2] = uint8_t__count;
    uint16_t__cursor = 3u;
    for (uint8_t__i = 0u; uint8_t__i < uint8_t__count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)(uint8_t__start + uint8_t__i);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_error_t__A__Errors[uint8_t__idx].uint32_t__timestamp);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_error_t__A__Errors[uint8_t__idx].uint32_t__faultMask);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_error_t__A__Errors[uint8_t__idx].uint32_t__vInMv);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_error_t__A__Errors[uint8_t__idx].uint32_t__vPackMv);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_error_t__A__Errors[uint8_t__idx].uint32_t__vLowMv);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_error_t__A__Errors[uint8_t__idx].uint32_t__vHighMv);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_error_t__A__Errors[uint8_t__idx].uint32_t__state1);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_error_t__A__Errors[uint8_t__idx].uint32_t__state2);
    }
    func__Diag_UnlockRecord(int32_t__savedLock);
    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_DIAG_DATA,
                            UINT8_T__A__Payload, uint16_t__cursor);
}
static void func__EspLink_SendDiagPower(uint8_t uint8_t__chunkId,
                                        uint8_t uint8_t__start,
                                        uint8_t uint8_t__count)
{
    uint8_t UINT8_T__A__Payload[3u + (10u * 12u)];
    uint16_t uint16_t__cursor = 0u;
    int32_t int32_t__savedLock;
    const diag_record_t *diag_record_t__rec;
    uint8_t uint8_t__i;
    if (uint8_t__count > 10u) uint8_t__count = 10u;
    if (uint8_t__start >= DIAG_MAX_POWER_EVENTS) return;
    if ((uint8_t__start + uint8_t__count) > DIAG_MAX_POWER_EVENTS)
    {
        uint8_t__count = (uint8_t)(DIAG_MAX_POWER_EVENTS - uint8_t__start);
    }
    int32_t__savedLock = func__Diag_LockRecord();
    diag_record_t__rec = func__Diag_GetRecord();
    UINT8_T__A__Payload[0] = uint8_t__chunkId;
    UINT8_T__A__Payload[1] = uint8_t__start;
    UINT8_T__A__Payload[2] = uint8_t__count;
    uint16_t__cursor = 3u;
    for (uint8_t__i = 0u; uint8_t__i < uint8_t__count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)(uint8_t__start + uint8_t__i);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_power_event_t__A__PowerEvents[uint8_t__idx].uint32_t__offTime);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_power_event_t__A__PowerEvents[uint8_t__idx].uint32_t__onTime);
        func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                             diag_record_t__rec->diag_power_event_t__A__PowerEvents[uint8_t__idx].uint32_t__durationSec);
    }
    func__Diag_UnlockRecord(int32_t__savedLock);
    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_DIAG_DATA,
                            UINT8_T__A__Payload, uint16_t__cursor);
}
/* ==================== EspLink_SendDiagData ==================== */
static void func__EspLink_SendDiagData(void)
{
    int32_t int32_t__savedLock = func__Diag_LockRecord();
    const diag_record_t *diag_record_t__rec = func__Diag_GetRecord();
    uint8_t uint8_t__err = diag_record_t__rec->uint8_t__errorCount;
    uint8_t uint8_t__pow = diag_record_t__rec->uint8_t__powerEventCount;
    func__Diag_UnlockRecord(int32_t__savedLock);
    uint8_t uint8_t__err1 = (uint8_t__err > 5u) ? 5u : uint8_t__err;
    uint8_t uint8_t__err2 = (uint8_t__err > 5u) ? (uint8_t)(uint8_t__err - 5u) : 0u;
    uint8_t uint8_t__pow1 = (uint8_t__pow > 10u) ? 10u : uint8_t__pow;
    uint8_t uint8_t__pow2 = (uint8_t__pow > 10u) ? (uint8_t)(uint8_t__pow - 10u) : 0u;
    func__EspLink_SendDiagChunk0();
    func__EspLink_SendDiagErrors(1u, 0u, uint8_t__err1);
    func__EspLink_SendDiagErrors(2u, 5u, uint8_t__err2);
    func__EspLink_SendDiagPower(3u, 0u, uint8_t__pow1);
    func__EspLink_SendDiagPower(4u, 10u, uint8_t__pow2);
}
#endif /* MODULE_DIAG */
/* ==================== Frame handling ==================== */
/**
 * @brief  [EN] Handle one complete, CRC-verified frame: apply a SET_PARAM
 *              (with a PARAM_REPORT reply of the applied value, or a PARAM_NAK
 *              when it is not applied - v2) or answer
 *              GET_PARAMS with PARAMS_BULK. Unknown types are dropped silently. CAL_REFERENCE (0x03) is no longer
 *              handled - the panel stopped sending it in v1.7 and the dead
 *              handler was removed in v2 to fit the flash budget.
* @param uint8_t__messageType Message type /
* @param uint16_t__payloadLength Payload length (v1.16 u16) / payload
 * @param  const uint8_t *uint8_t__payload [EN] Payload / payload
 */
/**
 * @brief  [EN] Answer one LUT stage (v1.66). Every single LUT frame is
 *              acknowledged - the panel can therefore say WHERE a push
 *              failed instead of only that it did.
 */
static void func__EspLink_SendLutAck(uint8_t uint8_t__stage,
                                     uint8_t uint8_t__status,
                                     uint32_t uint32_t__crc32)
{
    uint8_t UINT8_T__A__Payload[8u];
    UINT8_T__A__Payload[0] = uint8_t__stage;
    UINT8_T__A__Payload[1] = uint8_t__status;
    /*
 * [EN] Staged counts of THIS transaction, not the active table - the
 * sender matches them against its own push (see the statics above).
 */
    UINT8_T__A__Payload[2] = UINT8_T__G__LutStageN1;
    UINT8_T__A__Payload[3] = UINT8_T__G__LutStageN2;
    UINT8_T__A__Payload[4] = (uint8_t)(uint32_t__crc32 & 0xFFu);
    UINT8_T__A__Payload[5] = (uint8_t)((uint32_t__crc32 >> 8) & 0xFFu);
    UINT8_T__A__Payload[6] = (uint8_t)((uint32_t__crc32 >> 16) & 0xFFu);
    UINT8_T__A__Payload[7] = (uint8_t)((uint32_t__crc32 >> 24) & 0xFFu);
    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_LUT_ACK,
                            UINT8_T__A__Payload, 8u);
}
/* ==================== LUT active readback ==================== */
/**
 * @brief  [EN] Send one channel of the active LUT as point pairs. A zero-count
 *              frame is intentional: it tells the panel that this channel has
 *              no active flash override, rather than leaving stale values.
* @param uint8_t__channel Channel 1 or 2 /
* @return None /
 */
static void func__EspLink_SendLutData(uint8_t uint8_t__channel)
{
    static uint8_t UINT8_T__A__Payload[3u + (8u * CAL_LUT_POINTS_MAX)];
    const uint32_t *uint32_t__ptr_chain = func__CalLut_Counts(uint8_t__channel);
    const uint32_t *uint32_t__ptr_battery = func__CalLut_BatteryMa(uint8_t__channel);
    uint32_t uint32_t__points = func__CalLut_Points(uint8_t__channel);
    uint8_t uint8_t__count = (uint8_t)uint32_t__points;
    uint16_t uint16_t__cursor = 3u;
    uint8_t uint8_t__index;
    UINT8_T__A__Payload[0] = uint8_t__channel;
    UINT8_T__A__Payload[1] = 0u;
    UINT8_T__A__Payload[2] = uint8_t__count;
    if ((uint8_t__count > 0u) &&
        ((uint32_t__ptr_chain != NULL) && (uint32_t__ptr_battery != NULL)))
    {
        for (uint8_t__index = 0u; uint8_t__index < uint8_t__count; uint8_t__index++)
        {
            func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                                 uint32_t__ptr_chain[uint8_t__index]);
            func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor,
                                 uint32_t__ptr_battery[uint8_t__index]);
        }
    }
    else
    {
        /*
 * [EN] An inactive channel must produce a clean zero-point frame.
 */
        UINT8_T__A__Payload[2] = 0u;
        uint16_t__cursor = 3u;
    }
    func__EspLink_SendFrame((uint8_t)ESPLINK_MSG_LUT_DATA,
                            UINT8_T__A__Payload, uint16_t__cursor);
}
/* ==================== EspLink_HandleLutRead ==================== */
/**
 * @brief  [EN] Answer a zero-payload LUT_READ with both active channels.
* @param uint16_t__payloadLength Must be zero /
* @return bool true when this was a LUT read request /
 */
static bool func__EspLink_HandleLutRead(uint16_t uint16_t__payloadLength)
{
    if (uint16_t__payloadLength != 0u)
    {
        return false;
    }
    func__EspLink_SendLutData((uint8_t)CAL_LUT_CHANNEL_1);
    func__EspLink_SendLutData((uint8_t)CAL_LUT_CHANNEL_2);
    return true;
}
/* ==================== EspLink_RecordLutCommitAck ==================== */
/*
 * [EN] Keep the handshake policy as a small stateful predicate so it is
 * testable without a UART or flash model. A successful commit is the proof of
 * intent, including a valid zero-point commit that deliberately disables both
 * flash overrides; active-table presence is not a second gate.
 */
static void func__EspLink_RecordLutCommitAck(bool bool__success)
{
    BOOL__G__LutResetAuthorized = bool__success;
}
/* ==================== EspLink_ConsumeLutResetAuthorization ==================== */
static bool func__EspLink_ConsumeLutResetAuthorization(bool bool__magicValid)
{
    if ((bool__magicValid == false) ||
        (BOOL__G__LutResetAuthorized == false))
    {
        return false;
    }
    BOOL__G__LutResetAuthorized = false;
    return true;
}
/**
 * @brief  [EN] Handle the LUT push and active-read messages. Kept out of
 *              HandleFrame so the hot parameter path stays as short as it was.
* @return bool true = this type was a LUT frame /
 */
static bool func__EspLink_HandleLutFrame(uint8_t uint8_t__messageType,
                                         uint16_t uint16_t__payloadLength,
                                         const uint8_t *uint8_t__payload)
{
    if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_LUT_READ)
    {
        return func__EspLink_HandleLutRead(uint16_t__payloadLength);
    }
    if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_LUT_BEGIN)
    {
        uint8_t uint8_t__status;
        /*
 * [EN] The latest LUT ACK is no longer the successful commit once a
 * new staging transaction begins.
 */
        func__EspLink_RecordLutCommitAck(false);
        uint8_t__status = (uint8_t)CAL_LUT_ST_COUNT;
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
        uint8_t uint8_t__status;
        /*
 * [EN] A chunk ACK supersedes a commit ACK; reset must wait for a
 * fresh successful commit after this transaction step.
 */
        func__EspLink_RecordLutCommitAck(false);
        uint8_t__status = (uint8_t)CAL_LUT_ST_MISSING;
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
                    uint32_t uint32_t__counts =
                        func__EspLink_GetU32(uint8_t__payload, uint16_t__offset);
                    uint32_t uint32_t__batteryMa =
                        func__EspLink_GetU32(uint8_t__payload,
                                             (uint16_t)(uint16_t__offset + 4u));
                    if (func__CalLut_StagePoint(uint8_t__channel,
                                                (uint32_t)(uint8_t__first + uint8_t__i),
                                                uint32_t__counts,
                                                uint32_t__batteryMa) == false)
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
        /*
 * [EN] Literal 'R','S','T','!' and a fresh successful COMMIT are the
 * reset proof. A zero-point COMMIT is intentionally allowed: it clears
 * both flash overrides, so neither active-table predicate can be used
 * as a reset gate. The grant is consumed only after NVM flush succeeds.
 */
        if ((uint16_t__payloadLength == 4u) &&
            (uint8_t__payload[0] == (uint8_t)'R') &&
            (uint8_t__payload[1] == (uint8_t)'S') &&
            (uint8_t__payload[2] == (uint8_t)'T') &&
            (uint8_t__payload[3] == (uint8_t)'!'))
        {
            bool__magicValid = true;
        }
        if ((bool__magicValid != false) &&
            (BOOL__G__LutResetAuthorized != false))
        {
            /*
 * [EN] User bug 2026-10-07: the reboot must not outrun the
 * debounced save - flush a pending record BEFORE consuming the
 * commit grant and arming reset, so a reset right after a settings
 * edit keeps the new values. A failed flush is a visible flash
 * error and leaves the grant available for a retry.
 */
            if (func__EspLink_NvmFlushForReset() != false)
            {
                if (func__EspLink_ConsumeLutResetAuthorization(bool__magicValid) != false)
                {
                    func__CalLut_RequestReset();
                    uint8_t__status = (uint8_t)CAL_LUT_ST_OK;
                }
            }
            else
            {
                uint8_t__status = (uint8_t)CAL_LUT_ST_FLASH;
            }
        }
        func__EspLink_SendLutAck((uint8_t)ESPLINK_LUT_ACK_STAGE_RESET,
                                 uint8_t__status, func__CalLut_ActiveCrc32());
        return true;
    }
    return false;
}
/**
 * @brief  [EN] Handle one HELLO frame: answer it and nothing else. Kept as its
 *              own function so the "no side effect" rule is testable on the
 *              very code the dispatcher runs.
* @param uint8_t__payload HELLO payload / payload HELLO
* @param uint16_t__payloadLength Payload length, bytes / payload ()
* @return None /
 */
static void func__EspLink_HandleHelloFrame(const uint8_t *uint8_t__payload,
                                           uint16_t uint16_t__payloadLength)
{
    (void)func__EspLink_HandleHello(uint8_t__payload, uint16_t__payloadLength);
}
static void func__EspLink_HandleFrame(uint8_t uint8_t__messageType,
                                      uint16_t uint16_t__payloadLength,
                                      const uint8_t *uint8_t__payload)
{
    uint32_t uint32_t__value;
    uint32_t uint32_t__appliedValue;
    if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_HELLO)
    {
        /*
 * [EN] v1.111 handshake: answered here, first in the chain, so nothing
         *      else can delay the answer the panel is waiting for.
         *      Audited 2026-10-25: a HELLO must have NO side effect beyond the
         *      handshake state. It is not a user interaction. Routing it into
         *      NotifyWifiClientState(true) would hold the ESP awake forever on
         *      battery (a 2 s cadence against a 15 s client hold resets the
         *      30-minute idle timer every time, killing the whole power-saving
         *      schedule), and refreshing the charger's manual-mode dead-man on
         *      a HELLO would keep manual PWM alive for a panel that never sent
         *      a single SET_PARAM. Only real frames refresh those.
 */
        /*
 * [EN] Kept to ONE call on purpose: the host test drives this exact
         *      function, so any relation smuggled in here is caught.
 */
        func__EspLink_HandleHelloFrame(uint8_t__payload, uint16_t__payloadLength);
    }
    else if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_SET_PARAM)
    {
        if (uint16_t__payloadLength == 5u)
        {
            /*
 * [EN] Valid frame: refresh the manual-mode link dead-man
 * (protocol v1.2 - while manual is on, 3 s of silence makes
 * the charger drop both duties and resume autonomous mode).
 */
#if MODULE_CHARGER
            func__Charger_NotifyEspLinkActivity();
#endif /* MODULE_CHARGER */
            uint32_t uint32_t__previousValue = 0u;
            bool bool__hadPrevious =
                func__EspLink_GetParam(uint8_t__payload[0], &uint32_t__previousValue);
            uint32_t__value = func__EspLink_GetU32(uint8_t__payload, 1u);
            if (func__EspLink_ApplyParam(uint8_t__payload[0], uint32_t__value,
                                         &uint32_t__appliedValue) == false)
            {
                /* [EN] v2 NAK: the id was unknown or its owner refused. */
                func__EspLink_SendParamNak(uint8_t__payload[0], ESPLINK_NAK_NOT_APPLIED);
            }
            else
            {
                func__EspLink_NotifyWifiClientState(true);
                func__EspLink_SendParamReport(uint8_t__payload[0],
                                              uint32_t__appliedValue);
                /*
 * [EN] v1.14 (user order 2026-09-25: values must survive
 * power loss): a successfully applied PERSISTED parameter
 * arms the debounced flash save when the value changed
 * (transient test ids are filtered inside MarkDirty).
 */
                if ((bool__hadPrevious == false) ||
                    (uint32_t__appliedValue != uint32_t__previousValue))
                {
                    func__EspLink_NvmMarkDirty(uint8_t__payload[0]);
                }
            }
        }
        else
        {
            /* [EN] v2 NAK: wrong payload length (id unknown when the frame is empty). */
            func__EspLink_SendParamNak((uint16_t__payloadLength >= 1u) ? uint8_t__payload[0] : 0xFFu,
                                       ESPLINK_NAK_BAD_LENGTH);
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
    else if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_TIME_SYNC)
    {
#if MODULE_DIAG
        if (uint16_t__payloadLength == 4u)
        {
            uint32_t uint32_t__unix = func__EspLink_GetU32(uint8_t__payload, 0u);
            func__Diag_SetBoardTime(uint32_t__unix);
        }
#endif
#if MODULE_CHARGER
        func__Charger_NotifyEspLinkActivity();
#endif
    }
    else if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_DIAG_REQ)
    {
        if ((uint16_t__payloadLength == 1u) && (uint8_t__payload[0] == 1u))
        {
            func__EspLink_NotifyWifiClientState(true);
#if MODULE_DIAG
            func__Diag_ClearHistory();
#endif
        }
#if MODULE_DIAG
        func__EspLink_SendDiagData();
#endif
#if MODULE_CHARGER
        func__Charger_NotifyEspLinkActivity();
#endif
    }
    else if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_WIFI_CLIENT_STATE)
    {
        if (uint16_t__payloadLength == 1u)
        {
            func__EspLink_NotifyWifiClientState(uint8_t__payload[0] != 0u);
#if MODULE_CHARGER
            if (uint8_t__payload[0] != 0u)
            {
                func__Charger_NotifyEspLinkActivity();
            }
#endif
        }
    }
    else if (func__EspLink_HandleLutFrame(uint8_t__messageType,
                                          uint16_t__payloadLength,
                                          uint8_t__payload) != false)
    {
        /*
 * [EN] v1.66 LUT push handled (and acknowledged) above.
 */
        func__EspLink_NotifyWifiClientState(true);
#if MODULE_CHARGER
        func__Charger_NotifyEspLinkActivity();
#endif /* MODULE_CHARGER */
    }
    else
    {
        /*
 * [EN] Unknown message type: ignore and resync on the next frame.
 */
    }
}
/* ==================== EspLink_ResyncFromByte ==================== */
/*
 * [EN] Return to the earliest state that can still recognize the current
 * byte as the start of the next frame. Error bytes are data already consumed
 * by the failing frame, but AA itself may be the next SOF0; dropping it loses
 * an immediately following AA 55 and leaves the link blind until another
 * frame happens to arrive.
 */
static void func__EspLink_ResyncFromByte(uint8_t uint8_t__byte)
{
    ESP_LINK_PARSE_STATE_T__G__State =
        (uint8_t__byte == (uint8_t)ESPLINK_SOF_BYTE0)
            ? ESP_LINK_PARSE_WAIT_SOF1
            : ESP_LINK_PARSE_WAIT_SOF0;
}
/* ==================== EspLink_ParseByte ==================== */
/**
 * @brief  [EN] Feed one received byte into the parser state machine.
* @param uint8_t__byte Received byte /
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
                /*
 * [EN] AA AA 55: the second AA is itself a fresh SOF0
 * (full-program audit 2026-09-26) - stay in WAIT_SOF1
 * instead of dropping it, so one garbage byte cannot eat
 * a whole frame.
 */
            }
            else
            {
                ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
            }
            break;
        case ESP_LINK_PARSE_WAIT_VERSION:
            /*
 * [EN] A version we do not speak means the two boards were flashed
 * out of step. Parsing on would either fail on the CRC or, worse,
 * accept a frame whose layout has moved. Count it and resync, so
 * the condition is visible instead of looking like a dead link.
 */
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
            /*
 * [EN] v1.16: u16 little-endian length; the range check runs
 * once both bytes are in.
 */
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
                /*
 * [EN] Impossible length: resynchronize, retaining AA as
 * SOF0 when the failing high byte is itself AA.
 */
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
                /*
 * [EN] v1.111: every CRC-good frame is counted here, in BOTH
                 *      builds, because the handshake answer reports this number
                 *      to the panel: it is how the user sees that the board
                 *      really hears the ESP (a dead RX direction shows 0 even
                 *      while telemetry pours out of the board).
 */
                UINT32_T__G__RxFrameOkCount++;
#ifdef ESPLINK_HOST_TEST
                /* [EN] Host parser tests deliberately avoid the product
                   dispatch graph; acceptance itself is the observable.
                   WIFI_CLIENT_STATE (0x33) is also forwarded so host tests
                   can verify byte-level client state frames end to end. */
                UINT32_T__G__HostAcceptedFrames++;
                if ((UINT8_T__G__FrameType == (uint8_t)ESPLINK_MSG_WIFI_CLIENT_STATE) &&
                    (UINT16_T__G__FrameLen == 1u))
                {
                    func__EspLink_NotifyWifiClientState(UINT8_T__G__PayloadBuffer[0] != 0u);
                }
#else
                func__EspLink_HandleFrame(UINT8_T__G__FrameType,
                                          UINT16_T__G__FrameLen,
                                          UINT8_T__G__PayloadBuffer);
#endif
            }
            else
            {
                /*
 * [EN] Counted so a noisy link is measurable instead of just
 */
                UINT32_T__G__RxCrcError++;
            }
            /*
 * [EN] CRC-HI may be AA, which is already the next frame's
 * SOF0. Preserve it instead of requiring a third frame to
 * recover after AA 55.
 */
            func__EspLink_ResyncFromByte(uint8_t__byte);
            break;
        default:
            func__EspLink_ResyncFromByte(uint8_t__byte);
            break;
    }
}
#ifdef ESPLINK_HOST_TEST
/* ==================== EspLink_HostTest_Reset ==================== */
/** [EN] Host-only parser and handshake probes; never part of the firmware API. */
void func__EspLink_HostTest_Reset(void)
{
    ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
    UINT32_T__G__RxCrcError = 0u;
    UINT32_T__G__RxVersionMismatch = 0u;
    UINT32_T__G__HostAcceptedFrames = 0u;
    UINT32_T__G__HostChPdHighWrites = 0u;
    BOOL__G__HandshakeOk = false;
    UINT32_T__G__HandshakeAgeMs = 0u;
    UINT32_T__G__HandshakeCount = 0u;
    UINT32_T__G__HandshakeLostCount = 0u;
    UINT32_T__G__RxFrameOkCount = 0u;
    UINT8_T__G__EspFwVersion = 0u;
    UINT8_T__G__EspRole = 0u;
    UINT32_T__G__EspUptimeSec = 0u;
    BOOL__G__LutResetAuthorized = false;
    UINT8_T__G__LutStageN1 = 0u;
    UINT8_T__G__LutStageN2 = 0u;
    BOOL__G__EspPowerOn = true;
    BOOL__G__EspPinUp = true;
    BOOL__G__EspPowerOffPending = false;
    UINT32_T__G__EspPowerOffElapsedMs = 0u;
    BOOL__G__WifiClientConnected = false;
    UINT32_T__G__ClientActiveHoldMs = 0u;
    UINT32_T__G__BatteryIdleElapsedMs = 0u;
    UINT32_T__G__WifiIdleLowPowerMs = ESPLINK_WIFI_IDLE_LOW_POWER_MS;
    UINT32_T__G__WifiCycleOnMs = ESPLINK_WIFI_CYCLE_ON_MS;
    UINT32_T__G__WifiCycleOffMs = ESPLINK_WIFI_CYCLE_OFF_MS;
    UINT32_T__G__WifiBatCutoffPct = ESPLINK_WIFI_BAT_CUTOFF_PCT;
    UINT8_T__G__BatteryPercent = 100u;
    BOOL__G__LastInputPresent = true;
    BOOL__G__WifiBatCutoffLatched = false;
}
/* ==================== EspLink_HostTest_FeedByte ==================== */
void func__EspLink_HostTest_FeedByte(uint8_t uint8_t__byte)
{
    func__EspLink_ParseByte(uint8_t__byte);
}
/* ==================== EspLink_HostTest_CrcErrors ==================== */
uint32_t func__EspLink_HostTest_CrcErrors(void)
{
    return UINT32_T__G__RxCrcError;
}
/* ==================== EspLink_HostTest_VersionErrors ==================== */
uint32_t func__EspLink_HostTest_VersionErrors(void)
{
    return UINT32_T__G__RxVersionMismatch;
}
/* ==================== EspLink_HostTest_AcceptedFrames ==================== */
uint32_t func__EspLink_HostTest_AcceptedFrames(void)
{
    return UINT32_T__G__HostAcceptedFrames;
}
/* ==================== EspLink_HostTest_ChPdHighWrites ==================== */
/**
 * @brief  [EN] Number of HIGH requests sent to the physical CH_PD line since
 *              the last host reset. The v1.110 boot regression showed zero.
* @return uint32_t HIGH-request count /
 */
uint32_t func__EspLink_HostTest_ChPdHighWrites(void)
{
    return UINT32_T__G__HostChPdHighWrites;
}
/* ==================== EspLink_HostTest_HandleHello ==================== */
/**
 * @brief  [EN] Run the production HELLO path (state, counters, HELLO_ACK) from
 *              a host test without the whole module dispatch graph.
* @param uint8_t__payload HELLO payload / payload HELLO
* @param uint16_t__payloadLength Length /
* @return bool true when accepted / true
 */
bool func__EspLink_HostTest_HandleHello(const uint8_t *uint8_t__payload,
                                        uint16_t uint16_t__payloadLength)
{
    return func__EspLink_HandleHello(uint8_t__payload, uint16_t__payloadLength);
}
/* ==================== EspLink_HostTest_AdvanceHandshake ==================== */
/**
 * @brief  [EN] Advance the handshake timeout exactly like the comm task does.
* @param uint32_t__stepMs Step in ms / ms
 */
void func__EspLink_HostTest_AdvanceHandshake(uint32_t uint32_t__stepMs)
{
    func__EspLink_HandshakeTick(uint32_t__stepMs);
}
/* ==================== EspLink_HostTest_RxFrameOk ==================== */
/**
 * @brief  [EN] Accepted-frame counter the ACK reports.
* @return Count /
 */
uint32_t func__EspLink_HostTest_RxFrameOk(void)
{
    return UINT32_T__G__RxFrameOkCount;
}
/* ==================== EspLink_HostTest_HandleHelloFrame ==================== */
/**
 * @brief  [EN] Host probe: the production HELLO-frame handler the dispatcher
 *              calls. The host parser build stops before the product dispatch
 *              graph, so this is how the test proves a HELLO changes nothing
 *              except the handshake state.
* @param uint8_t__payload HELLO payload / payload HELLO
* @param uint16_t__length Payload length, bytes / payload ()
* @return None /
 */
void func__EspLink_HostTest_HandleHelloFrame(const uint8_t *uint8_t__payload,
                                             uint16_t uint16_t__length)
{
    func__EspLink_HandleHelloFrame(uint8_t__payload, uint16_t__length);
}
/* ==================== EspLink_HostTest_ClientHoldMs ==================== */
/**
 * @brief  [EN] Host probe: the Wi-Fi client hold left on the battery timer.
* @return uint32_t Remaining hold, ms / (ms)
 */
uint32_t func__EspLink_HostTest_ClientHoldMs(void)
{
    return UINT32_T__G__ClientActiveHoldMs;
}
/* ==================== EspLink_HostTest_WifiClientConnected ==================== */
/**
 * @brief  [EN] Host probe: does the power policy currently believe a user is
 *              connected (the flag that keeps the ESP awake on battery)?
* @return bool true when connected / true
 */
bool func__EspLink_HostTest_WifiClientConnected(void)
{
    return BOOL__G__WifiClientConnected;
}
/* ==================== EspLink_HostTest_RecordCommitAck ==================== */
void func__EspLink_HostTest_RecordCommitAck(bool bool__success)
{
    func__EspLink_RecordLutCommitAck(bool__success);
}
bool func__EspLink_HostTest_TryReset(bool bool__magicValid,
                                     bool bool__tableActive)
{
    /*
 * [EN] tableActive is retained in the host probe signature for source
 * compatibility; a successful zero-point commit is a valid reset grant.
 */
    (void)bool__tableActive;
    return func__EspLink_ConsumeLutResetAuthorization(bool__magicValid);
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
 */
void func__EspLink_Init(void)
{
#ifndef ESPLINK_HOST_TEST
    func__BspUart_Init();
#endif
    ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
    BOOL__G__LutResetAuthorized = false;
    UINT8_T__G__LutStageN1 = 0u;
    UINT8_T__G__LutStageN2 = 0u;
    UINT16_T__G__TelemetrySeq = 0u;
    /*
 * [EN] v1.110: the board port left CH_PD LOW (boot safe level), so the
     *      flags are reset to match BEFORE the power-on call below - that is
     *      what makes Power(true) actually drive the pin high. Resetting only
     *      the logical flag was the defect: the physical flag stayed true, the
     *      write was skipped and the ESP never started.
 */
    BOOL__G__EspPowerOn = false;
    BOOL__G__EspPinUp = false;
    BOOL__G__EspPowerOffPending = false;
    UINT32_T__G__EspPowerOffElapsedMs = 0u;
    BOOL__G__HandshakeOk = false;
    UINT32_T__G__HandshakeAgeMs = 0u;
    UINT32_T__G__HandshakeCount = 0u;
    UINT32_T__G__HandshakeLostCount = 0u;
    UINT32_T__G__RxFrameOkCount = 0u;
    UINT8_T__G__EspFwVersion = 0u;
    UINT8_T__G__EspRole = 0u;
    UINT32_T__G__EspUptimeSec = 0u;
    BOOL__G__WifiClientConnected = false;
    UINT32_T__G__ClientActiveHoldMs = 0u;
    UINT32_T__G__BatteryIdleElapsedMs = 0u;
    UINT8_T__G__BatteryPercent = 100u;
    BOOL__G__LastInputPresent = true;
    BOOL__G__WifiBatCutoffLatched = false;
    func__EspLink_Power(true);
}
/* ==================== EspLink_DriveChPd ==================== */
/**
 * @brief  [EN] Put one level on the physical CH_PD line. This is the ONLY
 *              place in the program that touches that pin, so the host build
 *              can count the HIGH requests and prove the ESP is switched on
 *              at boot (v1.110 regression guard).
 * @param  bool__level [EN] true = high (ESP enabled), false = off /
 */
static void func__EspLink_DriveChPd(bool bool__level)
{
#ifndef ESPLINK_HOST_TEST
    func__BspGpio_Write(BSP_GPIO_ESP_CHPD, bool__level);
#else
    if (bool__level != false)
    {
        UINT32_T__G__HostChPdHighWrites++;
    }
#endif
}
/* ==================== EspLink_Power ==================== */
/**
 * @brief  [EN] Drive CH_PD pin and reset the UART byte parser when waking
 *              from power-down.
* @param bool__on true=ESP on (HIGH), false=off / /
 */
void func__EspLink_Power(bool bool__on)
{
    if (bool__on != false)
    {
        BOOL__G__EspPowerOffPending = false;
        UINT32_T__G__EspPowerOffElapsedMs = 0u;
        if (BOOL__G__EspPowerOn == false)
        {
            ESP_LINK_PARSE_STATE_T__G__State = ESP_LINK_PARSE_WAIT_SOF0;
        }
        BOOL__G__EspPowerOn = true;
        if (BOOL__G__EspPinUp == false)
        {
            BOOL__G__EspPinUp = true;
            func__EspLink_DriveChPd(true);
        }
        return;
    }
    if (BOOL__G__EspPowerOn == false)
    {
        return;
    }
    BOOL__G__EspPowerOn = false;
    /*
 * [EN] The pin is NOT dropped here: a pending grace keeps the chip alive
 * for the last telemetry frames (see ESPLINK_POWER_OFF_GRACE_MS).
 */
    if ((BOOL__G__EspPinUp != false) && (BOOL__G__EspPowerOffPending == false))
    {
        BOOL__G__EspPowerOffPending = true;
        UINT32_T__G__EspPowerOffElapsedMs = 0u;
    }
}
/* ==================== EspLink_IsPowerOn ==================== */
/**
 * @brief  [EN] Query whether the ESP power rail (CH_PD) is currently enabled.
 * @return [EN] true if CH_PD is high, false if powered down /
 */
bool func__EspLink_IsPowerOn(void)
{
    return BOOL__G__EspPowerOn;
}
/* ==================== EspLink_IsPinPowered ==================== */
/**
 * @brief  [EN] Query the PHYSICAL CH_PD state: it can still be high for
 *              ESPLINK_POWER_OFF_GRACE_MS after the logical off, so the last
 *              telemetry frame reaches the panel.
* @return true if the pin is still up / true
 */
bool func__EspLink_IsPinPowered(void)
{
    return BOOL__G__EspPinUp;
}
/* ==================== EspLink_IsWifiClientConnected ==================== */
/**
 * @brief  [EN] Query whether a Wi-Fi client is currently considered connected.
 * @return [EN] true if a Wi-Fi client is active, false otherwise /
 */
bool func__EspLink_IsWifiClientConnected(void)
{
    bool bool__active = (BOOL__G__WifiClientConnected != false) ||
                        (UINT32_T__G__ClientActiveHoldMs > 0u);
    return bool__active;
}
/* ==================== EspLink_IsHandshakeOk ==================== */
/**
 * @brief  [EN] Handshake verdict for the rest of the firmware (v1.111).
* @return true while the ESP keeps answering / ESP
 */
bool func__EspLink_IsHandshakeOk(void)
{
    return BOOL__G__HandshakeOk;
}
/* ==================== EspLink_HandshakeCount ==================== */
/**
 * @brief  [EN] Accepted HELLO count since boot.
* @return Count /
 */
uint32_t func__EspLink_HandshakeCount(void)
{
    return UINT32_T__G__HandshakeCount;
}
/* ==================== EspLink_GetBatteryIdleElapsedMs ==================== */
/**
 * @brief  [EN] Return the elapsed battery idle time (ms) without a connected
 *              Wi-Fi user.
 * @return [EN] Elapsed idle milliseconds on battery /
 */
uint32_t func__EspLink_GetBatteryIdleElapsedMs(void)
{
    return UINT32_T__G__BatteryIdleElapsedMs;
}
/* ==================== EspLink_NotifyWifiClientState ==================== */
/**
 * @brief  [EN] Record the latest Wi-Fi client connection state reported by
 *              the ESP or inferred from an interactive user command.
 * @param  bool__connected [EN] true when a user is connected, false when idle /
 */
void func__EspLink_NotifyWifiClientState(bool bool__connected)
{
    if (bool__connected != false)
    {
        if ((BOOL__G__LastInputPresent == false) &&
            ((BOOL__G__WifiBatCutoffLatched != false) ||
             ((uint32_t)UINT8_T__G__BatteryPercent < UINT32_T__G__WifiBatCutoffPct)))
        {
            BOOL__G__WifiClientConnected = false;
            UINT32_T__G__ClientActiveHoldMs = 0u;
            return;
        }
        BOOL__G__WifiClientConnected = true;
        UINT32_T__G__ClientActiveHoldMs = (uint32_t)ESPLINK_WIFI_CLIENT_HOLD_MS;
        UINT32_T__G__BatteryIdleElapsedMs = 0u;
        if (BOOL__G__EspPowerOn == false)
        {
            func__EspLink_Power(true);
        }
    }
    else
    {
        BOOL__G__WifiClientConnected = false;
        UINT32_T__G__ClientActiveHoldMs = 0u;
    }
}
/* ==================== EspLink_SetBatteryPercent ==================== */
/**
 * @brief  [EN] Update the latest battery state-of-charge percentage (0..100)
 *              used by the Wi-Fi battery cutoff policy.
 * @param  uint8_t__batteryPct [EN] Battery percentage (0..100) /
 */
void func__EspLink_SetBatteryPercent(uint8_t uint8_t__batteryPct)
{
    if (uint8_t__batteryPct > 100u)
    {
        UINT8_T__G__BatteryPercent = 100u;
    }
    else
    {
        UINT8_T__G__BatteryPercent = uint8_t__batteryPct;
    }
}
/* ==================== EspLink_GetBatteryPercent ==================== */
/**
 * @brief  [EN] Read the battery state-of-charge percentage (0..100) currently
 *              seen by the Wi-Fi power-saving policy.
* @return Battery percentage (0..100) /
 */
uint8_t func__EspLink_GetBatteryPercent(void)
{
    return UINT8_T__G__BatteryPercent;
}
/* ==================== EspLink_PowerSaveTick ==================== */
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
 * @param  bool__inputPresent [EN] true if mains input is present /
 * @param  uint32_t__stepMs   [EN] Elapsed step in milliseconds /
 */
void func__EspLink_PowerSaveTick(bool bool__inputPresent,
                                 uint32_t uint32_t__stepMs)
{
    bool bool__manualActive = false;
    bool bool__clientActive;
    BOOL__G__LastInputPresent = bool__inputPresent;
    /*
 * [EN] Apply a pending power-off once the grace has elapsed. During the
 * grace the pin is still up, so EspLink_Run keeps sending telemetry.
 */
    if (BOOL__G__EspPowerOffPending != false)
    {
        UINT32_T__G__EspPowerOffElapsedMs += uint32_t__stepMs;
        if (UINT32_T__G__EspPowerOffElapsedMs >= ESPLINK_POWER_OFF_GRACE_MS)
        {
            BOOL__G__EspPowerOffPending = false;
            UINT32_T__G__EspPowerOffElapsedMs = 0u;
            BOOL__G__EspPinUp = false;
            func__EspLink_DriveChPd(false);
        }
    }
#if MODULE_CHARGER && !defined(ESPLINK_HOST_TEST)
    bool__manualActive = func__Charger_IsManualTestModeActive();
#endif
    if ((bool__inputPresent != false) ||
        (bool__manualActive != false) ||
        (UINT32_T__G__WifiBatCutoffPct == 0u))
    {
        BOOL__G__WifiBatCutoffLatched = false;
    }
    else
    {
        if ((uint32_t)UINT8_T__G__BatteryPercent < UINT32_T__G__WifiBatCutoffPct)
        {
            BOOL__G__WifiBatCutoffLatched = true;
        }
        else if (BOOL__G__WifiBatCutoffLatched != false)
        {
            uint32_t uint32_t__recoverPct = UINT32_T__G__WifiBatCutoffPct + 2u;
            if (uint32_t__recoverPct > 100u)
            {
                uint32_t__recoverPct = 100u;
            }
            if ((uint32_t)UINT8_T__G__BatteryPercent >= uint32_t__recoverPct)
            {
                BOOL__G__WifiBatCutoffLatched = false;
            }
        }
        else
        {
        }
        if (BOOL__G__WifiBatCutoffLatched != false)
        {
            BOOL__G__WifiClientConnected = false;
            UINT32_T__G__ClientActiveHoldMs = 0u;
            if (BOOL__G__EspPowerOn != false)
            {
                func__EspLink_Power(false);
            }
            return;
        }
    }
    if (UINT32_T__G__ClientActiveHoldMs > uint32_t__stepMs)
    {
        UINT32_T__G__ClientActiveHoldMs -= uint32_t__stepMs;
    }
    else
    {
        UINT32_T__G__ClientActiveHoldMs = 0u;
        BOOL__G__WifiClientConnected = false;
    }
    bool__clientActive = (BOOL__G__WifiClientConnected != false) ||
                         (UINT32_T__G__ClientActiveHoldMs > 0u);
    if ((bool__inputPresent != false) ||
        (bool__manualActive != false) ||
        (bool__clientActive != false))
    {
        UINT32_T__G__BatteryIdleElapsedMs = 0u;
        if (BOOL__G__EspPowerOn == false)
        {
            func__EspLink_Power(true);
        }
        return;
    }
    if ((UINT32_MAX - UINT32_T__G__BatteryIdleElapsedMs) > uint32_t__stepMs)
    {
        UINT32_T__G__BatteryIdleElapsedMs += uint32_t__stepMs;
    }
    else
    {
        UINT32_T__G__BatteryIdleElapsedMs = UINT32_MAX;
    }
    if ((UINT32_T__G__BatteryIdleElapsedMs < UINT32_T__G__WifiIdleLowPowerMs) ||
        (UINT32_T__G__WifiCycleOffMs == 0u))
    {
        if (BOOL__G__EspPowerOn == false)
        {
            func__EspLink_Power(true);
        }
    }
    else
    {
        uint32_t uint32_t__afterIdleMs =
            UINT32_T__G__BatteryIdleElapsedMs - UINT32_T__G__WifiIdleLowPowerMs;
        uint32_t uint32_t__cyclePeriodMs =
            UINT32_T__G__WifiCycleOffMs + UINT32_T__G__WifiCycleOnMs;
        uint32_t uint32_t__cycleOffsetMs =
            uint32_t__afterIdleMs % uint32_t__cyclePeriodMs;
        bool bool__shouldBeOn =
            (uint32_t__cycleOffsetMs >= UINT32_T__G__WifiCycleOffMs);
        if (BOOL__G__EspPowerOn != bool__shouldBeOn)
        {
            func__EspLink_Power(bool__shouldBeOn);
        }
    }
}
/* ==================== EspLink_Run ==================== */
/**
 * @brief  [EN] Consume every received byte (each complete command frame is
 *              applied immediately), update the hybrid Wi-Fi power-saving
 *              state machine, and send one TLM_LIVE telemetry frame while
 *              the ESP is powered. Called once per communication task period.
* @param measurement_snapshot_t__snap Snapshot /
* @param fault_mask_t__faults Fault bits /
 */
void func__EspLink_Run(const measurement_snapshot_t *measurement_snapshot_t__snap,
                       fault_mask_t fault_mask_t__faults)
{
    uint8_t uint8_t__byte;
    bool bool__inputPresent = true;
    while (func__BspUart_ReadByte(&uint8_t__byte) != false)
    {
        func__EspLink_ParseByte(uint8_t__byte);
    }
    if ((measurement_snapshot_t__snap != NULL) &&
        (measurement_snapshot_t__snap->valid != false))
    {
        bool__inputPresent = (measurement_snapshot_t__snap->input_present != false);
#if MODULE_UI && !defined(ESPLINK_HOST_TEST)
        func__EspLink_SetBatteryPercent(
            func__Ui_BatteryVoltageToPercent(measurement_snapshot_t__snap->v_bat24_mv));
#else
        if (measurement_snapshot_t__snap->v_bat24_mv <= 21000u)
        {
            func__EspLink_SetBatteryPercent(0u);
        }
        else if (measurement_snapshot_t__snap->v_bat24_mv >= 29000u)
        {
            func__EspLink_SetBatteryPercent(100u);
        }
        else
        {
            uint32_t uint32_t__deltaMv = measurement_snapshot_t__snap->v_bat24_mv - 21000u;
            uint32_t uint32_t__scaled = uint32_t__deltaMv * 100u;
            uint32_t uint32_t__pct = uint32_t__scaled / 8000u;
            func__EspLink_SetBatteryPercent((uint8_t)uint32_t__pct);
        }
#endif
    }
    func__EspLink_PowerSaveTick(bool__inputPresent, APP_CONFIG.comm_period_ms);
    /*
 * [EN] v1.111: the handshake timeout is advanced on the same fixed comm
     *      cadence as the power policy - no RTOS clock call, so the host test
     *      drives exactly the same code path.
 */
    func__EspLink_HandshakeTick(APP_CONFIG.comm_period_ms);
    /*
 * [EN] Gated on the PHYSICAL pin: during the power-off grace the last
 * frames still go out so the panel can log the mains cut.
 */
    if (BOOL__G__EspPinUp != false)
    {
        func__EspLink_SendTelemetry(measurement_snapshot_t__snap,
                                    fault_mask_t__faults);
    }
    /*
 * [EN] Debounced flash save of the persisted parameters. A save
 * stalls the WHOLE CPU ~30..50 ms (F1 single bank: SysTick jumps,
 * EXTI/UART ISRs go latent, in-flight UART bytes overrun) - not just
 * this task. The charger is suspended (gates at 0) around the save,
 * so no switching happens inside the blind window; the stall itself
 * is inherent to the part.
 */
    func__EspLink_NvmTick();
    /*
 * [EN] v1.66: performs the reboot the panel armed AFTER a verified LUT
 * push (the ACK frame has already left the UART by now).
 */
    func__CalLut_Tick();
}
