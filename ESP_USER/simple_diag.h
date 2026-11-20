#include <LittleFS.h>
/*
 * ==================== JSON ====================
 */
static uint8_t func__Simple_JsonReadNumbers(const String &string__content,
                                            int32_t int32_t__startPos,
                                            uint32_t *uint32_t__ptr_out,
                                            uint8_t uint8_t__maxCount)
{
    uint8_t uint8_t__count = 0u;
    int32_t int32_t__pos = int32_t__startPos;
    int32_t int32_t__length = (int32_t)string__content.length();
    if (int32_t__startPos < 0) return 0u;
    while ((int32_t__pos < int32_t__length) && (string__content.charAt((unsigned int)int32_t__pos) != '['))
    {
        int32_t__pos++;
    }
    while ((int32_t__pos < int32_t__length) && (uint8_t__count < uint8_t__maxCount))
    {
        int32_t int32_t__endPos;
        String string__value;
        char char__ch = string__content.charAt((unsigned int)int32_t__pos);
        if ((char__ch == '[') || (char__ch == ','))
        {
            int32_t__pos++;
            continue;
        }
        if (char__ch == ']') break;
        int32_t__endPos = int32_t__pos;
        while (int32_t__endPos < int32_t__length)
        {
            char char__inner = string__content.charAt((unsigned int)int32_t__endPos);
            if ((char__inner == ',') || (char__inner == ']')) break;
            int32_t__endPos++;
        }
        string__value = string__content.substring(int32_t__pos, int32_t__endPos);
        string__value.trim();
        if (string__value.length() > 0u)
        {
            uint32_t__ptr_out[uint8_t__count] = (uint32_t)string__value.toInt();
            uint8_t__count++;
        }
        int32_t__pos = int32_t__endPos;
    }
    return uint8_t__count;
}
static void func__Simple_ParamsSave(void)
{
    uint8_t uint8_t__i;
    if (!BOOL__G__FsOk) return;
    String string__data;
    if (!func__Simple_StorageTextAppend(string__data, String("{\"pv\":["))) return;
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_PARAM_UI_COUNT; uint8_t__i++)
    {
        uint8_t uint8_t__id = UINT8_T__G__ParamUiIds[uint8_t__i];
        if (uint8_t__i != 0u) if (!func__Simple_StorageTextAppend(string__data, String(","))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(uint8_t__id))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(","))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(func__Simple_BoardParam(uint8_t__id)))) return;
    }
    if (!func__Simple_StorageTextAppend(string__data, String("]}"))) return;
    if (func__Simple_StorageWrite(SIMPLE_PARAMS_FILE, string__data)) BOOL__G__BoardParamsDirty = false;
}
static void func__Simple_ParamsLoad(void)
{
    uint32_t uint32_t__values[SIMPLE_PARAM_UI_COUNT * 2u];
    uint8_t uint8_t__read;
    uint8_t uint8_t__i;
    if (!BOOL__G__FsOk) return;
    if (!LittleFS.exists(SIMPLE_PARAMS_FILE)) return;
    File file__f = LittleFS.open(SIMPLE_PARAMS_FILE, "r");
    if (!file__f) return;
    String string__content = file__f.readString();
    file__f.close();
    uint8_t__read = func__Simple_JsonReadNumbers(string__content,
        string__content.indexOf("\"pv\":["), uint32_t__values, (uint8_t)(SIMPLE_PARAM_UI_COUNT * 2u));
    for (uint8_t__i = 0u; uint8_t__i + 1u < uint8_t__read; uint8_t__i += 2u)
    {
        func__Simple_StoreBoardParam((uint8_t)uint32_t__values[uint8_t__i], uint32_t__values[uint8_t__i + 1u]);
    }
    BOOL__G__BoardParamsDirty = false;
}
static void func__Simple_ChartAdd(uint32_t uint32_t__vin, uint32_t uint32_t__vpack, uint32_t uint32_t__pct, uint32_t uint32_t__imb)
{
    UINT32_T__G__ChartVin[UINT8_T__G__ChartIndex] = uint32_t__vin;
    UINT32_T__G__ChartVpack[UINT8_T__G__ChartIndex] = uint32_t__vpack;
    UINT32_T__G__ChartPct[UINT8_T__G__ChartIndex] = uint32_t__pct;
    UINT32_T__G__ChartImb[UINT8_T__G__ChartIndex] = uint32_t__imb;
    UINT8_T__G__ChartIndex = (uint8_t)((UINT8_T__G__ChartIndex + 1u) % SIMPLE_CHART_HISTORY_SIZE);
    if (UINT8_T__G__ChartCount < SIMPLE_CHART_HISTORY_SIZE)
    {
        UINT8_T__G__ChartCount++;
    }
}
/* ==================== - +Vin+Vpack ==================== */
static void func__Simple_ChartPushRange(uint32_t *uint32_t__ptr_pct,
                                        uint32_t *uint32_t__ptr_vin,
                                        uint32_t *uint32_t__ptr_vpack,
                                        uint32_t *uint32_t__ptr_time,
                                        uint16_t *uint16_t__ptr_count, uint16_t *uint16_t__ptr_index,
                                        uint16_t uint16_t__size, uint32_t uint32_t__nowSec,
                                        uint32_t uint32_t__pct, uint32_t uint32_t__vin,
                                        uint32_t uint32_t__vpack)
{
    uint16_t uint16_t__slot = *uint16_t__ptr_index;
    if (*uint16_t__ptr_count > 0u)
    {
        uint16_t uint16_t__newestSlot = (uint16_t)((*uint16_t__ptr_index + uint16_t__size - 1u) % uint16_t__size);
        if (uint32_t__nowSec <= uint32_t__ptr_time[uint16_t__newestSlot]) return;
    }
    BOOL__G__ChartsDirty = true;
    uint32_t__ptr_pct[uint16_t__slot] = uint32_t__pct;
    uint32_t__ptr_vin[uint16_t__slot] = uint32_t__vin;
    uint32_t__ptr_vpack[uint16_t__slot] = uint32_t__vpack;
    uint32_t__ptr_time[uint16_t__slot] = uint32_t__nowSec;
    *uint16_t__ptr_index = (uint16_t)((uint16_t__slot + 1u) % uint16_t__size);
    if (*uint16_t__ptr_count < uint16_t__size)
    {
        *uint16_t__ptr_count = (uint16_t)(*uint16_t__ptr_count + 1u);
    }
}
static uint32_t func__Simple_ChartLastSec(const uint32_t *uint32_t__ptr_time,
                                          uint16_t uint16_t__count,
                                          uint16_t uint16_t__index,
                                          uint16_t uint16_t__size)
{
    uint16_t uint16_t__newestSlot;
    if (uint16_t__count == 0u) return 0u;
    uint16_t__newestSlot = (uint16_t)((uint16_t__index + uint16_t__size - 1u) % uint16_t__size);
    return uint32_t__ptr_time[uint16_t__newestSlot];
}
#include "simple_chart_store.h"
/* ==================== - STM32 ==================== */
static void func__Simple_LocalRecordFault(uint32_t uint32_t__faultMask, uint32_t uint32_t__vin, uint32_t uint32_t__vpack, uint32_t uint32_t__vLow, uint32_t uint32_t__vHigh)
{
    if (!func__Simple_TimeBootReady() || uint32_t__faultMask == 0u) return;
    uint32_t uint32_t__newBits = uint32_t__faultMask & (uint32_t)(~UINT32_T__G__LastLocalFaultMask);
    if (uint32_t__newBits == 0u) return;
    uint32_t uint32_t__nowSec = func__Simple_NowSec();
    uint8_t uint8_t__i;
    for (uint8_t__i = SIMPLE_DIAG_MAX_ERRORS - 1u; uint8_t__i > 0u; uint8_t__i--)
    {
        SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i] = SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i - 1u];
    }
    SIMPLE_DIAG_ERROR_T__G__Errors[0].timestamp = uint32_t__nowSec;
    SIMPLE_DIAG_ERROR_T__G__Errors[0].faultMask = uint32_t__faultMask;
    SIMPLE_DIAG_ERROR_T__G__Errors[0].vIn = uint32_t__vin;
    SIMPLE_DIAG_ERROR_T__G__Errors[0].vPack = uint32_t__vpack;
    SIMPLE_DIAG_ERROR_T__G__Errors[0].vLow = uint32_t__vLow;
    SIMPLE_DIAG_ERROR_T__G__Errors[0].vHigh = uint32_t__vHigh;
    SIMPLE_DIAG_ERROR_T__G__Errors[0].state1 = UINT32_T__G__TlmField[6];
    SIMPLE_DIAG_ERROR_T__G__Errors[0].state2 = UINT32_T__G__TlmField[13];
    if (UINT8_T__G__DiagErrorCount < SIMPLE_DIAG_MAX_ERRORS) UINT8_T__G__DiagErrorCount++;
    BOOL__G__DiagDirty = true;
    BOOL__G__DiagValid = true;
}
static void func__Simple_LocalRecordPower(bool bool__inputPresent)
{
    if (!func__Simple_TimeBootReady())
    {
        BOOL__G__LastLocalInputValid = false;
        BOOL__G__LocalPowerOffActive = false;
        return;
    }
    uint32_t uint32_t__nowSec = func__Simple_NowSec();
    if (!BOOL__G__LastLocalInputValid)
    {
        BOOL__G__LastLocalInputPresent = bool__inputPresent;
        BOOL__G__LastLocalInputValid = true;
        return;
    }
    if (BOOL__G__LastLocalInputPresent && !bool__inputPresent)
    {
        if (!BOOL__G__LocalPowerOffActive)
        {
            BOOL__G__LocalPowerOffActive = true;
            UINT32_T__G__LocalPowerOffTime = uint32_t__nowSec;
        }
    }
    else if (!BOOL__G__LastLocalInputPresent && bool__inputPresent)
    {
        if (BOOL__G__LocalPowerOffActive)
        {
            BOOL__G__LocalPowerOffActive = false;
            uint8_t uint8_t__i;
            for (uint8_t__i = SIMPLE_DIAG_MAX_POWER_EVENTS - 1u; uint8_t__i > 0u; uint8_t__i--)
            {
                SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i] = SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i - 1u];
            }
            SIMPLE_POWER_EVENT_T__G__PowerEvents[0].offTime = UINT32_T__G__LocalPowerOffTime;
            SIMPLE_POWER_EVENT_T__G__PowerEvents[0].onTime = uint32_t__nowSec;
            if (uint32_t__nowSec >= UINT32_T__G__LocalPowerOffTime)
                SIMPLE_POWER_EVENT_T__G__PowerEvents[0].durationSec = uint32_t__nowSec - UINT32_T__G__LocalPowerOffTime;
            else
                SIMPLE_POWER_EVENT_T__G__PowerEvents[0].durationSec = 0u;
            if (UINT8_T__G__PowerEventCount < SIMPLE_DIAG_MAX_POWER_EVENTS) UINT8_T__G__PowerEventCount++;
            BOOL__G__DiagDirty = true;
            BOOL__G__DiagValid = true;
        }
    }
    BOOL__G__LastLocalInputPresent = bool__inputPresent;
}
static void func__Simple_ChartTick(void)
{
    uint32_t uint32_t__now = (uint32_t)millis();
    if ((uint32_t__now - UINT32_T__G__LastChartMs) < 5000u) return;
    UINT32_T__G__LastChartMs = uint32_t__now;
    bool bool__tlmFresh = (BOOL__G__TlmSeen != false) &&
        (((uint32_t)(uint32_t__now - UINT32_T__G__LastTlmMs)) <= SIMPLE_LINK_TIMEOUT_MS);
    uint32_t uint32_t__vin = 0u;
    uint32_t uint32_t__vpack = 0u;
    uint32_t uint32_t__pct = 0u;
    uint32_t uint32_t__imb = 0u;
    if (bool__tlmFresh)
    {
        uint32_t__vin = UINT32_T__G__TlmField[14];
        uint32_t__vpack = UINT32_T__G__TlmField[15];
        uint32_t__imb = UINT32_T__G__TlmField[25];
        uint32_t uint32_t__vMinMv = func__Simple_BoardParam(SIMPLE_PARAM_PCT_VMIN_MV);
        uint32_t uint32_t__vMaxMv = func__Simple_BoardParam(SIMPLE_PARAM_PCT_VMAX_MV);
        /* Stored chart percentages retain the discharge scale, independent of charge bounds. */
        if ((uint32_t__vMaxMv > uint32_t__vMinMv) && (uint32_t__vpack >= uint32_t__vMinMv))
        {
            uint32_t uint32_t__voltageAboveMinMv = uint32_t__vpack - uint32_t__vMinMv;
            uint32_t uint32_t__voltageRangeMv = uint32_t__vMaxMv - uint32_t__vMinMv;
            if (uint32_t__voltageAboveMinMv >= uint32_t__voltageRangeMv)
            {
                uint32_t__pct = 100u;
            }
            else
            {
                uint32_t uint32_t__scaledPercent = uint32_t__voltageAboveMinMv * 100u;
                uint32_t__pct = uint32_t__scaledPercent / uint32_t__voltageRangeMv;
            }
        }
    }
    func__Simple_ChartAdd(uint32_t__vin, uint32_t__vpack, uint32_t__pct, uint32_t__imb);
    func__Simple_ChartsSave();
    if (bool__tlmFresh) func__Simple_ChartAddTimed(uint32_t__pct, uint32_t__vin, uint32_t__vpack);
    if (!bool__tlmFresh) return;
    uint32_t uint32_t__vLow = UINT32_T__G__TlmField[17];
    uint32_t uint32_t__vHigh = UINT32_T__G__TlmField[18];
    uint32_t uint32_t__faultMask = UINT32_T__G__TlmField[19];
    if (uint32_t__faultMask != 0u)
    {
        func__Simple_LocalRecordFault(uint32_t__faultMask, uint32_t__vin, uint32_t__vpack, uint32_t__vLow, uint32_t__vHigh);
    }
    UINT32_T__G__LastLocalFaultMask = uint32_t__faultMask;
    bool bool__inputPresent = true;
    if ((UINT8_T__G__TlmFlags & 0x02u) == 0u) bool__inputPresent = false;
    if (uint32_t__vin < 18000u) bool__inputPresent = false;
    func__Simple_LocalRecordPower(bool__inputPresent);
}
static void func__Simple_SendTimeSync(uint32_t uint32_t__unixSec)
{
    uint8_t UINT8_T__A__Payload[4];
    UINT8_T__A__Payload[0] = (uint8_t)(uint32_t__unixSec & 0xFFu);
    UINT8_T__A__Payload[1] = (uint8_t)((uint32_t__unixSec >> 8) & 0xFFu);
    UINT8_T__A__Payload[2] = (uint8_t)((uint32_t__unixSec >> 16) & 0xFFu);
    UINT8_T__A__Payload[3] = (uint8_t)((uint32_t__unixSec >> 24) & 0xFFu);
    uint8_t UINT8_T__A__Frame[16];
    uint16_t uint16_t__crc = SIMPLE_LINK_CRC16_INIT;
    UINT8_T__A__Frame[0] = SIMPLE_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = SIMPLE_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = SIMPLE_LINK_PROTOCOL_VERSION;
    UINT8_T__A__Frame[3] = SIMPLE_MSG_TIME_SYNC;
    UINT8_T__A__Frame[4] = 4u;
    UINT8_T__A__Frame[5] = 0u;
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_LINK_PROTOCOL_VERSION);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_MSG_TIME_SYNC);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 4u);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < 4u; uint8_t__i++)
    {
        UINT8_T__A__Frame[6u + uint8_t__i] = UINT8_T__A__Payload[uint8_t__i];
        uint16_t__crc = func__Simple_Crc16(uint16_t__crc, UINT8_T__A__Payload[uint8_t__i]);
    }
    UINT8_T__A__Frame[10] = (uint8_t)(uint16_t__crc & 0xFFu);
    UINT8_T__A__Frame[11] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
    (void)Serial.write(UINT8_T__A__Frame, 12u);
}
static void func__Simple_SendDiagReq(void)
{
    uint8_t UINT8_T__A__Frame[9];
    uint16_t uint16_t__crc = SIMPLE_LINK_CRC16_INIT;
    uint8_t uint8_t__len = BOOL__G__DiagClearPending ? 1u : 0u;
    UINT8_T__A__Frame[0] = SIMPLE_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = SIMPLE_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = SIMPLE_LINK_PROTOCOL_VERSION;
    UINT8_T__A__Frame[3] = SIMPLE_MSG_DIAG_REQ;
    UINT8_T__A__Frame[4] = uint8_t__len;
    UINT8_T__A__Frame[5] = 0u;
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_LINK_PROTOCOL_VERSION);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_MSG_DIAG_REQ);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, uint8_t__len);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    if (uint8_t__len == 1u)
    {
        UINT8_T__A__Frame[6] = 1u;
        uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 1u);
        UINT8_T__A__Frame[7] = (uint8_t)(uint16_t__crc & 0xFFu);
        UINT8_T__A__Frame[8] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
        (void)Serial.write(UINT8_T__A__Frame, 9u);
    }
    else
    {
        UINT8_T__A__Frame[6] = (uint8_t)(uint16_t__crc & 0xFFu);
        UINT8_T__A__Frame[7] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
        (void)Serial.write(UINT8_T__A__Frame, 8u);
    }
    UINT32_T__G__LastDiagReqMs = (uint32_t)millis();
}
static void func__Simple_SendGetParams(void)
{
    uint8_t UINT8_T__A__Frame[8];
    uint16_t uint16_t__crc = SIMPLE_LINK_CRC16_INIT;
    UINT8_T__A__Frame[0] = SIMPLE_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = SIMPLE_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = SIMPLE_LINK_PROTOCOL_VERSION;
    UINT8_T__A__Frame[3] = SIMPLE_MSG_GET_PARAMS;
    UINT8_T__A__Frame[4] = 0u;
    UINT8_T__A__Frame[5] = 0u;
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_LINK_PROTOCOL_VERSION);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_MSG_GET_PARAMS);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    UINT8_T__A__Frame[6] = (uint8_t)(uint16_t__crc & 0xFFu);
    UINT8_T__A__Frame[7] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
    (void)Serial.write(UINT8_T__A__Frame, 8u);
    UINT32_T__G__LastParamsReqMs = (uint32_t)millis();
}
#ifndef SIMPLE_CHARGE_HISTORY_FILE
#define SIMPLE_CHARGE_HISTORY_FILE      "/charge_hist.json"
#endif
/* ==================== Charge snapshot write ==================== */
/** @brief Commit bounded version-two session rows through the shared atomic writer. */
static void func__Simple_ChargeHistorySave(void)
{
    if (!BOOL__G__FsOk) return;
    String string__data;
    if (!func__Simple_StorageTextAppend(string__data, String("{\"v\":2,\"hist\":["))) return;
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UINT8_T__G__ChargeHistoryCount; uint8_t__i++)
    {
        const simple_charge_event_t &event = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i];
        char char__row[96];
        int int__n = snprintf(char__row, sizeof(char__row), "%s[%lu,%u,%lu,%lu,%u]",
            uint8_t__i ? "," : "", (unsigned long)event.timestamp, event.channel,
            (unsigned long)event.durationSec, (unsigned long)event.startSec, event.status);
        if (int__n <= 0 || (size_t)int__n >= sizeof(char__row)) return;
        if (!func__Simple_StorageTextAppend(string__data, String(char__row))) return;
    }
    if (!func__Simple_StorageTextAppend(string__data, String("]}"))) return;
    if (func__Simple_StorageWrite(SIMPLE_CHARGE_HISTORY_FILE, string__data)) BOOL__G__ChargeHistDirty = false;
}
/* ==================== Strict snapshot numbers ==================== */
/** @brief Read one unsigned decimal and required delimiter; reject overflow and junk. */
static bool func__Simple_ChargeNumber(const String &text, int &pos, uint32_t &value, char delimiter)
{
    value = 0u;
    int int__digits = 0;
    while (pos < (int)text.length())
    {
        char ch = text.charAt((unsigned)pos);
        if (ch < '0' || ch > '9') break;
        uint32_t digit = (uint32_t)(ch - '0');
        if (value > (UINT32_MAX - digit) / 10u) return false;
        value = value * 10u + digit;
        pos++;
        int__digits++;
    }
    if (!int__digits || text.charAt((unsigned)pos) != delimiter) return false;
    pos++;
    return true;
}
/* ==================== Charge snapshot read ==================== */
/** @brief Load only complete version-two snapshots; interrupted observation stays unknown. */
static void func__Simple_ChargeHistoryLoad(void)
{
    UINT8_T__G__ChargeHistoryCount = 0u;
    func__Simple_ChargeReset();
    if (!BOOL__G__FsOk || !LittleFS.exists(SIMPLE_CHARGE_HISTORY_FILE)) return;
    File file = LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE, "r");
    if (!file || file.size() > SIMPLE_STORAGE_HISTORY_BYTES) return;
    String text = file.readString();
    file.close();
    const String prefix("{\"v\":2,\"hist\":[");
    if (text.substring(0, prefix.length()) != prefix) return;
    int pos = (int)prefix.length();
    uint8_t count = 0u;
    bool recovered = false;
    while (text.charAt((unsigned)pos) == '[' && count < SIMPLE_DIAG_MAX_POWER_EVENTS)
    {
        pos++;
        uint32_t value[5];
        for (uint8_t i = 0u; i < 5u; i++)
            if (!func__Simple_ChargeNumber(text, pos, value[i], i < 4u ? ',' : ']')) return;
        if (value[0] < SIMPLE_TIME_VALID_MIN || value[1] < 1u || value[1] > 2u || value[4] > 3u ||
            (value[3] && (value[3] < SIMPLE_TIME_VALID_MIN || value[3] > value[0]))) return;
        if (value[4] == 3u) { value[4] = 0u; value[2] = 0u; recovered = true; }
        if (!value[3] || value[4] == 0u) value[2] = 0u;
        SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[count++] = {value[0], (uint8_t)value[1], value[2], value[3], (uint8_t)value[4]};
        if (text.charAt((unsigned)pos) != ',') break;
        pos++;
        if (text.charAt((unsigned)pos) != '[') return;
    }
    if (text.substring(pos) != "]}") return;
    UINT8_T__G__ChargeHistoryCount = count;
    if (recovered) BOOL__G__ChargeHistDirty = true;
}
#ifndef SIMPLE_DIAG_FILE
#define SIMPLE_DIAG_FILE            "/diag.json"
#endif
static void func__Simple_DiagSave(void)
{
    if (!BOOL__G__FsOk) return;
    String string__data;
    if (!func__Simple_StorageTextAppend(string__data, String("{\"ec\":"))) return;
    if (!func__Simple_StorageTextAppend(string__data, String(UINT8_T__G__DiagErrorCount))) return;
    if (!func__Simple_StorageTextAppend(string__data, String(",\"pc\":"))) return;
    if (!func__Simple_StorageTextAppend(string__data, String(UINT8_T__G__PowerEventCount))) return;
    if (!func__Simple_StorageTextAppend(string__data, String(",\"err\":["))) return;
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__DiagErrorCount; uint8_t__i++)
    {
        if (uint8_t__i != 0u) if (!func__Simple_StorageTextAppend(string__data, String(","))) return;
        if (!func__Simple_StorageTextAppend(string__data, String("{\"ts\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].timestamp))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(",\"fm\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].faultMask))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(",\"vin\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vIn))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(",\"vp\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vPack))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(",\"vl\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vLow))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(",\"vh\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vHigh))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(",\"s1\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].state1))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(",\"s2\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].state2))) return;
        if (!func__Simple_StorageTextAppend(string__data, String("}"))) return;
    }
    if (!func__Simple_StorageTextAppend(string__data, String("],\"pwr\":["))) return;
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__PowerEventCount; uint8_t__i++)
    {
        if (uint8_t__i != 0u) if (!func__Simple_StorageTextAppend(string__data, String(","))) return;
        if (!func__Simple_StorageTextAppend(string__data, String("{\"off\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i].offTime))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(",\"on\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i].onTime))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(",\"dur\":"))) return;
        if (!func__Simple_StorageTextAppend(string__data, String(SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i].durationSec))) return;
        if (!func__Simple_StorageTextAppend(string__data, String("}"))) return;
    }
    if (!func__Simple_StorageTextAppend(string__data, String("]}"))) return;
    if (func__Simple_StorageWrite(SIMPLE_DIAG_FILE, string__data)) BOOL__G__DiagDirty = false;
}
static void func__Simple_DiagLoad(void)
{
    if (!BOOL__G__FsOk) return;
    if (!LittleFS.exists(SIMPLE_DIAG_FILE)) return;
    File file__f = LittleFS.open(SIMPLE_DIAG_FILE, "r");
    if (!file__f) return;
    String string__content = file__f.readString();
    file__f.close();
    if (string__content.length() < 10) return;
    int32_t int32_t__errS = string__content.indexOf("\"err\":[");
    if (int32_t__errS >= 0)
    {
        int32_t int32_t__errE = string__content.indexOf("]", int32_t__errS);
        if (int32_t__errE > int32_t__errS)
        {
            String string__errArr = string__content.substring(int32_t__errS + 7, int32_t__errE + 1);
            int32_t int32_t__pos = 0;
            uint8_t uint8_t__cnt = 0u;
            while (uint8_t__cnt < SIMPLE_DIAG_MAX_ERRORS)
            {
                int32_t int32_t__ts = string__errArr.indexOf("\"ts\":", int32_t__pos);
                if (int32_t__ts < 0) break;
                int32_t__ts += 5;
                int32_t int32_t__tsEnd = string__errArr.indexOf(",", int32_t__ts);
                if (int32_t__tsEnd < 0) break;
                int32_t int32_t__fm = string__errArr.indexOf("\"fm\":", int32_t__tsEnd);
                if (int32_t__fm < 0) break;
                int32_t__fm += 5;
                int32_t int32_t__fmEnd = string__errArr.indexOf(",", int32_t__fm);
                if (int32_t__fmEnd < 0) break;
                int32_t int32_t__vin = string__errArr.indexOf("\"vin\":", int32_t__fmEnd);
                if (int32_t__vin < 0) break;
                int32_t__vin += 6;
                int32_t int32_t__vinEnd = string__errArr.indexOf(",", int32_t__vin);
                int32_t int32_t__vp = string__errArr.indexOf("\"vp\":", int32_t__vinEnd);
                if (int32_t__vp < 0) break;
                int32_t__vp += 5;
                int32_t int32_t__vpEnd = string__errArr.indexOf(",", int32_t__vp);
                int32_t int32_t__vl = string__errArr.indexOf("\"vl\":", int32_t__vpEnd);
                if (int32_t__vl < 0) break;
                int32_t__vl += 5;
                int32_t int32_t__vlEnd = string__errArr.indexOf(",", int32_t__vl);
                int32_t int32_t__vh = string__errArr.indexOf("\"vh\":", int32_t__vlEnd);
                if (int32_t__vh < 0) break;
                int32_t__vh += 5;
                int32_t int32_t__objEnd = string__errArr.indexOf("}", int32_t__vh);
                if (int32_t__objEnd < 0) break;
                int32_t int32_t__vhComma = string__errArr.indexOf(",", int32_t__vh);
                int32_t int32_t__vhEnd = ((int32_t__vhComma >= 0) && (int32_t__vhComma < int32_t__objEnd)) ? int32_t__vhComma : int32_t__objEnd;
                uint32_t uint32_t__s1 = 0u;
                uint32_t uint32_t__s2 = 0u;
                int32_t int32_t__s1 = string__errArr.indexOf("\"s1\":", int32_t__vhEnd);
                if ((int32_t__s1 >= 0) && (int32_t__s1 < int32_t__objEnd))
                {
                    int32_t__s1 += 5;
                    int32_t int32_t__s1Comma = string__errArr.indexOf(",", int32_t__s1);
                    int32_t int32_t__s1End = ((int32_t__s1Comma >= 0) && (int32_t__s1Comma < int32_t__objEnd)) ? int32_t__s1Comma : int32_t__objEnd;
                    uint32_t__s1 = (uint32_t)string__errArr.substring(int32_t__s1, int32_t__s1End).toInt();
                }
                int32_t int32_t__s2 = string__errArr.indexOf("\"s2\":", int32_t__vhEnd);
                if ((int32_t__s2 >= 0) && (int32_t__s2 < int32_t__objEnd))
                {
                    int32_t__s2 += 5;
                    int32_t int32_t__s2Comma = string__errArr.indexOf(",", int32_t__s2);
                    int32_t int32_t__s2End = ((int32_t__s2Comma >= 0) && (int32_t__s2Comma < int32_t__objEnd)) ? int32_t__s2Comma : int32_t__objEnd;
                    uint32_t__s2 = (uint32_t)string__errArr.substring(int32_t__s2, int32_t__s2End).toInt();
                }
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].timestamp = (uint32_t)string__errArr.substring(int32_t__ts, int32_t__tsEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].faultMask = (uint32_t)string__errArr.substring(int32_t__fm, int32_t__fmEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].vIn = (uint32_t)string__errArr.substring(int32_t__vin, int32_t__vinEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].vPack = (uint32_t)string__errArr.substring(int32_t__vp, int32_t__vpEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].vLow = (uint32_t)string__errArr.substring(int32_t__vl, int32_t__vlEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].vHigh = (uint32_t)string__errArr.substring(int32_t__vh, int32_t__vhEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].state1 = uint32_t__s1;
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].state2 = uint32_t__s2;
                uint8_t__cnt++;
                int32_t__pos = int32_t__objEnd + 1;
            }
            if (uint8_t__cnt > 0)
            {
                UINT8_T__G__DiagErrorCount = uint8_t__cnt;
                BOOL__G__DiagValid = true;
            }
        }
    }
    int32_t int32_t__pwrS = string__content.indexOf("\"pwr\":[");
    if (int32_t__pwrS >= 0)
    {
        int32_t int32_t__pwrE = string__content.indexOf("]", int32_t__pwrS);
        if (int32_t__pwrE > int32_t__pwrS)
        {
            String string__pwrArr = string__content.substring(int32_t__pwrS + 7, int32_t__pwrE + 1);
            int32_t int32_t__pos = 0;
            uint8_t uint8_t__cnt = 0u;
            while (uint8_t__cnt < SIMPLE_DIAG_MAX_POWER_EVENTS)
            {
                int32_t int32_t__off = string__pwrArr.indexOf("\"off\":", int32_t__pos);
                if (int32_t__off < 0) break;
                int32_t__off += 6;
                int32_t int32_t__offEnd = string__pwrArr.indexOf(",", int32_t__off);
                if (int32_t__offEnd < 0) break;
                int32_t int32_t__on = string__pwrArr.indexOf("\"on\":", int32_t__offEnd);
                if (int32_t__on < 0) break;
                int32_t__on += 5;
                int32_t int32_t__onEnd = string__pwrArr.indexOf(",", int32_t__on);
                int32_t int32_t__dur = string__pwrArr.indexOf("\"dur\":", int32_t__onEnd);
                if (int32_t__dur < 0) break;
                int32_t__dur += 6;
                int32_t int32_t__durEnd = string__pwrArr.indexOf("}", int32_t__dur);
                if (int32_t__durEnd < 0) break;
                SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__cnt].offTime = (uint32_t)string__pwrArr.substring(int32_t__off, int32_t__offEnd).toInt();
                SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__cnt].onTime = (uint32_t)string__pwrArr.substring(int32_t__on, int32_t__onEnd).toInt();
                SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__cnt].durationSec = (uint32_t)string__pwrArr.substring(int32_t__dur, int32_t__durEnd).toInt();
                uint8_t__cnt++;
                int32_t__pos = int32_t__durEnd + 1;
            }
            if (uint8_t__cnt > 0)
            {
                if (UINT8_T__G__PowerEventCount == 0u) UINT8_T__G__PowerEventCount = uint8_t__cnt;
                else if (uint8_t__cnt > UINT8_T__G__PowerEventCount) UINT8_T__G__PowerEventCount = uint8_t__cnt;
                BOOL__G__DiagValid = true;
            }
        }
    }
}
static bool func__Simple_ChargeHistoryClear(bool bool__persist = true)
{
    String string__empty("{\"v\":2,\"hist\":[]}");
    if (bool__persist && !func__Simple_StorageWrite(SIMPLE_CHARGE_HISTORY_FILE, string__empty)) return false;
    UINT8_T__G__ChargeHistoryCount = 0u;
    func__Simple_ChargeReset();
    UINT32_T__G__PrevChargeCountCh1 = UINT32_T__G__ChargeCountCh1;
    UINT32_T__G__PrevChargeCountCh2 = UINT32_T__G__ChargeCountCh2;
    BOOL__G__ChargeCountBaselineReady = BOOL__G__DiagValid;
    BOOL__G__ChargeHistDirty = false;
    return true;
}
static bool func__Simple_ChartsClear(bool bool__persist = true)
{
    /* Empty snapshot supersedes even a complete unacknowledged journal tail. */
    uint32_t uint32_t__sequence = UINT32_T__G__ChartSequence;
    if (bool__persist && uint32_t__sequence == UINT32_MAX) return false;
    uint32_t__sequence = bool__persist ? uint32_t__sequence + 1u : 0u;
    uint8_t uint8_t__empty[SIMPLE_CHART_FILE_HEADER + 19u + 2u] = {};
    (void)memcpy(uint8_t__empty, "CHTD\2\4\0\0", 8u);
    for (uint8_t uint8_t__i = 0u; uint8_t__i < 4u; uint8_t__i++)
        uint8_t__empty[8u + uint8_t__i] = (uint8_t)(uint32_t__sequence >> (8u * uint8_t__i));
    uint16_t uint16_t__crc = SimpleLogCodec::crc(uint8_t__empty, sizeof(uint8_t__empty) - 2u);
    uint8_t__empty[sizeof(uint8_t__empty) - 2u] = (uint8_t)uint16_t__crc;
    uint8_t__empty[sizeof(uint8_t__empty) - 1u] = (uint8_t)(uint16_t__crc >> 8);
    if (bool__persist && !func__Simple_StorageCommit(SIMPLE_CHARTS_FILE, uint8_t__empty, sizeof(uint8_t__empty))) return false;
    func__Simple_ChartStoreReset(false);
    UINT32_T__G__ChartSequence = uint32_t__sequence;
    UINT32_T__G__ChartSnapshotSequence = uint32_t__sequence;
    UINT8_T__G__ChartCount = 0u;
    UINT8_T__G__ChartIndex = 0u;
    UINT16_T__G__Chart1D_Count = 0u; UINT16_T__G__Chart1D_Index = 0u;
    UINT16_T__G__Chart7D_Count = 0u; UINT16_T__G__Chart7D_Index = 0u;
    UINT16_T__G__Chart1M_Count = 0u; UINT16_T__G__Chart1M_Index = 0u;
    UINT16_T__G__Chart3M_Count = 0u; UINT16_T__G__Chart3M_Index = 0u;
    UINT32_T__G__Last1D_Ms = 0u; UINT32_T__G__Last7D_Ms = 0u; UINT32_T__G__Last1M_Ms = 0u; UINT32_T__G__Last3M_Ms = 0u;
    UINT32_T__G__LastChartSaveSec = 0u;
    BOOL__G__ChartsDirty = false;
    if (bool__persist) (void)LittleFS.remove(SIMPLE_CHART_JOURNAL_FILE);
    return true;
}
static bool func__Simple_DiagClear(bool bool__persist = true)
{
    if (bool__persist && !func__Simple_StorageWrite(SIMPLE_DIAG_FILE, String("{\"ec\":0,\"pc\":0,\"err\":[],\"pwr\":[]}"))) return false;
    UINT8_T__G__DiagErrorCount = 0u;
    UINT8_T__G__PowerEventCount = 0u;
    UINT32_T__G__LastLocalFaultMask = UINT32_T__G__TlmField[19];
    BOOL__G__LastLocalInputValid = false;
    BOOL__G__LocalPowerOffActive = false;
    BOOL__G__DiagClearPending = true;
    BOOL__G__DiagDirty = false;
    func__Simple_SendDiagReq();
    return true;
}
