/* simple_diag.h - مدیریت دیاگ و تاریخچه نمودار با محور زمان - ۱ روزه/۷ روزه/۱ ماهه/۳ ماهه
   فقط فارسی - ذخیره کامل Vin/Vpack/Pct/Time برای همه بازه‌ها */

#include <LittleFS.h>

/* ==================== افزودن نقطه به نمودار کوتاه ==================== */
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

/* ==================== افزودن نقطه با زمان - درصد+Vin+Vpack برای ۳ ماه با ریت کم ==================== */
static void func__Simple_ChartAddTimed(uint32_t uint32_t__pct, uint32_t uint32_t__vin, uint32_t uint32_t__vpack)
{
    uint32_t uint32_t__nowSec = 0u;
    if (UINT32_T__G__BoardTimeSec != 0u)
    {
        uint32_t__nowSec = UINT32_T__G__BoardTimeSec;
    }
    else
    {
        uint32_t__nowSec = (uint32_t)(millis() / 1000u);
    }

    uint32_t uint32_t__nowMs = (uint32_t)millis();

    /* ۱ روزه - ۲۴ نقطه
       برای پر شدن سریع اولیه هر ۶۰ ثانیه، بعد از پر شدن هر ۱ ساعت */
    {
        uint32_t uint32_t__interval = (UINT8_T__G__Chart1D_Count < SIMPLE_CHART_1D_SIZE) ? 60000u : 3600000u;
        if ((uint32_t__nowMs - UINT32_T__G__Last1D_Ms) >= uint32_t__interval || UINT8_T__G__Chart1D_Count == 0u)
        {
            UINT32_T__G__Chart1D_Pct[UINT8_T__G__Chart1D_Index] = uint32_t__pct;
            UINT32_T__G__Chart1D_Vin[UINT8_T__G__Chart1D_Index] = uint32_t__vin;
            UINT32_T__G__Chart1D_Vpack[UINT8_T__G__Chart1D_Index] = uint32_t__vpack;
            UINT32_T__G__Chart1D_Time[UINT8_T__G__Chart1D_Index] = uint32_t__nowSec;
            UINT8_T__G__Chart1D_Index = (uint8_t)((UINT8_T__G__Chart1D_Index + 1u) % SIMPLE_CHART_1D_SIZE);
            if (UINT8_T__G__Chart1D_Count < SIMPLE_CHART_1D_SIZE) UINT8_T__G__Chart1D_Count++;
            UINT32_T__G__Last1D_Ms = uint32_t__nowMs;
        }
    }

    /* ۷ روزه - ۲۸ نقطه
       اولیه هر ۵ دقیقه، بعد هر ۶ ساعت */
    {
        uint32_t uint32_t__interval = (UINT8_T__G__Chart7D_Count < SIMPLE_CHART_7D_SIZE) ? 300000u : 21600000u;
        if ((uint32_t__nowMs - UINT32_T__G__Last7D_Ms) >= uint32_t__interval || UINT8_T__G__Chart7D_Count == 0u)
        {
            UINT32_T__G__Chart7D_Pct[UINT8_T__G__Chart7D_Index] = uint32_t__pct;
            UINT32_T__G__Chart7D_Vin[UINT8_T__G__Chart7D_Index] = uint32_t__vin;
            UINT32_T__G__Chart7D_Vpack[UINT8_T__G__Chart7D_Index] = uint32_t__vpack;
            UINT32_T__G__Chart7D_Time[UINT8_T__G__Chart7D_Index] = uint32_t__nowSec;
            UINT8_T__G__Chart7D_Index = (uint8_t)((UINT8_T__G__Chart7D_Index + 1u) % SIMPLE_CHART_7D_SIZE);
            if (UINT8_T__G__Chart7D_Count < SIMPLE_CHART_7D_SIZE) UINT8_T__G__Chart7D_Count++;
            UINT32_T__G__Last7D_Ms = uint32_t__nowMs;
        }
    }

    /* ۱ ماهه - ۳۰ نقطه - اولیه هر ۱۵ دقیقه، بعد هر ۱ روز */
    {
        uint32_t uint32_t__interval = (UINT8_T__G__Chart1M_Count < SIMPLE_CHART_1M_SIZE) ? 900000u : 86400000u;
        if ((uint32_t__nowMs - UINT32_T__G__Last1M_Ms) >= uint32_t__interval || UINT8_T__G__Chart1M_Count == 0u)
        {
            UINT32_T__G__Chart1M_Pct[UINT8_T__G__Chart1M_Index] = uint32_t__pct;
            UINT32_T__G__Chart1M_Vin[UINT8_T__G__Chart1M_Index] = uint32_t__vin;
            UINT32_T__G__Chart1M_Vpack[UINT8_T__G__Chart1M_Index] = uint32_t__vpack;
            UINT32_T__G__Chart1M_Time[UINT8_T__G__Chart1M_Index] = uint32_t__nowSec;
            UINT8_T__G__Chart1M_Index = (uint8_t)((UINT8_T__G__Chart1M_Index + 1u) % SIMPLE_CHART_1M_SIZE);
            if (UINT8_T__G__Chart1M_Count < SIMPLE_CHART_1M_SIZE) UINT8_T__G__Chart1M_Count++;
            UINT32_T__G__Last1M_Ms = uint32_t__nowMs;
        }
    }

    /* ۳ ماهه - ۹۰ نقطه - اولیه هر ۳۰ دقیقه، بعد هر ۱ روز */
    {
        uint32_t uint32_t__interval = (UINT8_T__G__Chart3M_Count < SIMPLE_CHART_3M_SIZE) ? 1800000u : 86400000u;
        if ((uint32_t__nowMs - UINT32_T__G__Last3M_Ms) >= uint32_t__interval || UINT8_T__G__Chart3M_Count == 0u)
        {
            UINT32_T__G__Chart3M_Pct[UINT8_T__G__Chart3M_Index] = uint32_t__pct;
            UINT32_T__G__Chart3M_Vin[UINT8_T__G__Chart3M_Index] = uint32_t__vin;
            UINT32_T__G__Chart3M_Vpack[UINT8_T__G__Chart3M_Index] = uint32_t__vpack;
            UINT32_T__G__Chart3M_Time[UINT8_T__G__Chart3M_Index] = uint32_t__nowSec;
            UINT8_T__G__Chart3M_Index = (uint8_t)((UINT8_T__G__Chart3M_Index + 1u) % SIMPLE_CHART_3M_SIZE);
            if (UINT8_T__G__Chart3M_Count < SIMPLE_CHART_3M_SIZE) UINT8_T__G__Chart3M_Count++;
            UINT32_T__G__Last3M_Ms = uint32_t__nowMs;
        }
    }
}

/* ==================== تیک نمودار هر 5 ثانیه ==================== */
static void func__Simple_ChartTick(void)
{
    uint32_t uint32_t__now = (uint32_t)millis();
    if ((uint32_t__now - UINT32_T__G__LastChartMs) < 5000u) return;
    UINT32_T__G__LastChartMs = uint32_t__now;

    if (!BOOL__G__TlmSeen) return;

    uint32_t uint32_t__vin = UINT32_T__G__TlmField[14];
    uint32_t uint32_t__vpackRaw = UINT32_T__G__TlmField[15];
    uint32_t uint32_t__vLow = UINT32_T__G__TlmField[17];
    uint32_t uint32_t__vHigh = UINT32_T__G__TlmField[18];
    uint32_t uint32_t__vpack = uint32_t__vpackRaw;
    if ((uint32_t__vLow > 0u) && (uint32_t__vHigh > 0u))
    {
        uint32_t__vpack = uint32_t__vLow + uint32_t__vHigh;
    }
    uint32_t uint32_t__pct = 0u;
    if (uint32_t__vpack >= 21000u && uint32_t__vpack <= 29000u)
    {
        uint32_t__pct = ((uint32_t__vpack - 21000u) * 100u) / (29000u - 21000u);
    }
    else if (uint32_t__vpack > 29000u)
    {
        uint32_t__pct = 100u;
    }
    uint32_t uint32_t__imb = UINT32_T__G__TlmField[25];
    func__Simple_ChartAdd(uint32_t__vin, uint32_t__vpack, uint32_t__pct, uint32_t__imb);
    func__Simple_ChartAddTimed(uint32_t__pct, uint32_t__vin, uint32_t__vpack);
}

/* ==================== ساخت فریم زمان ==================== */
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

/* ==================== درخواست دیاگ ==================== */
static void func__Simple_SendDiagReq(void)
{
    uint8_t UINT8_T__A__Frame[8];
    uint16_t uint16_t__crc = SIMPLE_LINK_CRC16_INIT;
    UINT8_T__A__Frame[0] = SIMPLE_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = SIMPLE_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = SIMPLE_LINK_PROTOCOL_VERSION;
    UINT8_T__A__Frame[3] = SIMPLE_MSG_DIAG_REQ;
    UINT8_T__A__Frame[4] = 0u;
    UINT8_T__A__Frame[5] = 0u;
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_LINK_PROTOCOL_VERSION);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_MSG_DIAG_REQ);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    UINT8_T__A__Frame[6] = (uint8_t)(uint16_t__crc & 0xFFu);
    UINT8_T__A__Frame[7] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
    (void)Serial.write(UINT8_T__A__Frame, 8u);
    UINT32_T__G__LastDiagReqMs = (uint32_t)millis();
}

/* ==================== ذخیره تاریخچه شارژ ==================== */
#define SIMPLE_CHARGE_HISTORY_FILE      "/charge_hist.json"
static void func__Simple_ChargeHistorySave(void)
{
    if (!BOOL__G__FsOk) return;
    File file__f = LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE, "w");
    if (!file__f) return;
    file__f.print("{\"count\":");
    file__f.print(UINT8_T__G__ChargeHistoryCount);
    file__f.print(",\"prevCh1\":");
    file__f.print(UINT32_T__G__PrevChargeCountCh1);
    file__f.print(",\"prevCh2\":");
    file__f.print(UINT32_T__G__PrevChargeCountCh2);
    file__f.print(",\"hist\":[");
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__ChargeHistoryCount; uint8_t__i++)
    {
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print("{\"ts\":");
        file__f.print(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i].timestamp);
        file__f.print(",\"ch\":");
        file__f.print(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i].channel);
        file__f.print("}");
    }
    file__f.print("]}");
    file__f.close();
}

static void func__Simple_ChargeHistoryLoad(void)
{
    UINT8_T__G__ChargeHistoryCount = 0u;
    UINT32_T__G__PrevChargeCountCh1 = 0u;
    UINT32_T__G__PrevChargeCountCh2 = 0u;
    if (!BOOL__G__FsOk) return;
    if (!LittleFS.exists(SIMPLE_CHARGE_HISTORY_FILE)) return;
    File file__f = LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE, "r");
    if (!file__f) return;
    String string__content = file__f.readString();
    file__f.close();
    int32_t int32_t__prev1Pos = string__content.indexOf("\"prevCh1\":");
    if (int32_t__prev1Pos >= 0)
    {
        int32_t__prev1Pos += 10;
        int32_t int32_t__prev1End = string__content.indexOf(",", int32_t__prev1Pos);
        if (int32_t__prev1End > int32_t__prev1Pos)
        {
            String string__p1 = string__content.substring(int32_t__prev1Pos, int32_t__prev1End);
            UINT32_T__G__PrevChargeCountCh1 = (uint32_t)string__p1.toInt();
        }
    }
    int32_t int32_t__prev2Pos = string__content.indexOf("\"prevCh2\":");
    if (int32_t__prev2Pos >= 0)
    {
        int32_t__prev2Pos += 10;
        int32_t int32_t__prev2End = string__content.indexOf(",", int32_t__prev2Pos);
        if (int32_t__prev2End > int32_t__prev2Pos)
        {
            String string__p2 = string__content.substring(int32_t__prev2Pos, int32_t__prev2End);
            UINT32_T__G__PrevChargeCountCh2 = (uint32_t)string__p2.toInt();
        }
    }
    int32_t int32_t__pos = 0;
    uint8_t uint8_t__found = 0u;
    while (uint8_t__found < SIMPLE_DIAG_MAX_POWER_EVENTS)
    {
        int32_t int32_t__tsPos = string__content.indexOf("\"ts\":", int32_t__pos);
        if (int32_t__tsPos < 0) break;
        int32_t__tsPos += 5;
        int32_t int32_t__tsEnd = string__content.indexOf(",", int32_t__tsPos);
        if (int32_t__tsEnd < 0) break;
        String string__ts = string__content.substring(int32_t__tsPos, int32_t__tsEnd);
        int32_t int32_t__chPos = string__content.indexOf("\"ch\":", int32_t__tsEnd);
        if (int32_t__chPos < 0) break;
        int32_t__chPos += 5;
        int32_t int32_t__chEnd = string__content.indexOf("}", int32_t__chPos);
        if (int32_t__chEnd < 0) int32_t__chEnd = string__content.length();
        String string__ch = string__content.substring(int32_t__chPos, int32_t__chEnd);
        SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__found].timestamp = (uint32_t)string__ts.toInt();
        SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__found].channel = (uint8_t)string__ch.toInt();
        SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__found].durationSec = 0u;
        uint8_t__found++;
        int32_t__pos = int32_t__chEnd;
    }
    UINT8_T__G__ChargeHistoryCount = uint8_t__found;
}

/* ==================== ذخیره نمودارها - کامل با Vin/Vpack ==================== */
static void func__Simple_ChartsSave(void)
{
    if (!BOOL__G__FsOk) return;
    File file__f = LittleFS.open(SIMPLE_CHARTS_FILE, "w");
    if (!file__f) return;

    file__f.print("{\"1d\":{\"c\":");
    file__f.print(UINT8_T__G__Chart1D_Count);
    file__f.print(",\"pct\":[");
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1D_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - UINT8_T__G__Chart1D_Count + uint8_t__i) % SIMPLE_CHART_1D_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart1D_Pct[uint8_t__idx]);
    }
    file__f.print("],\"vin\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1D_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - UINT8_T__G__Chart1D_Count + uint8_t__i) % SIMPLE_CHART_1D_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart1D_Vin[uint8_t__idx]);
    }
    file__f.print("],\"vpack\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1D_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - UINT8_T__G__Chart1D_Count + uint8_t__i) % SIMPLE_CHART_1D_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart1D_Vpack[uint8_t__idx]);
    }
    file__f.print("],\"time\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1D_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - UINT8_T__G__Chart1D_Count + uint8_t__i) % SIMPLE_CHART_1D_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart1D_Time[uint8_t__idx]);
    }
    file__f.print("]},\"7d\":{\"c\":");
    file__f.print(UINT8_T__G__Chart7D_Count);
    file__f.print(",\"pct\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart7D_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart7D_Index + SIMPLE_CHART_7D_SIZE - UINT8_T__G__Chart7D_Count + uint8_t__i) % SIMPLE_CHART_7D_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart7D_Pct[uint8_t__idx]);
    }
    file__f.print("],\"vin\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart7D_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart7D_Index + SIMPLE_CHART_7D_SIZE - UINT8_T__G__Chart7D_Count + uint8_t__i) % SIMPLE_CHART_7D_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart7D_Vin[uint8_t__idx]);
    }
    file__f.print("],\"vpack\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart7D_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart7D_Index + SIMPLE_CHART_7D_SIZE - UINT8_T__G__Chart7D_Count + uint8_t__i) % SIMPLE_CHART_7D_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart7D_Vpack[uint8_t__idx]);
    }
    file__f.print("],\"time\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart7D_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart7D_Index + SIMPLE_CHART_7D_SIZE - UINT8_T__G__Chart7D_Count + uint8_t__i) % SIMPLE_CHART_7D_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart7D_Time[uint8_t__idx]);
    }
    file__f.print("]},\"1m\":{\"c\":");
    file__f.print(UINT8_T__G__Chart1M_Count);
    file__f.print(",\"pct\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1M_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1M_Index + SIMPLE_CHART_1M_SIZE - UINT8_T__G__Chart1M_Count + uint8_t__i) % SIMPLE_CHART_1M_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart1M_Pct[uint8_t__idx]);
    }
    file__f.print("],\"vin\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1M_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1M_Index + SIMPLE_CHART_1M_SIZE - UINT8_T__G__Chart1M_Count + uint8_t__i) % SIMPLE_CHART_1M_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart1M_Vin[uint8_t__idx]);
    }
    file__f.print("],\"vpack\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1M_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1M_Index + SIMPLE_CHART_1M_SIZE - UINT8_T__G__Chart1M_Count + uint8_t__i) % SIMPLE_CHART_1M_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart1M_Vpack[uint8_t__idx]);
    }
    file__f.print("],\"time\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1M_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1M_Index + SIMPLE_CHART_1M_SIZE - UINT8_T__G__Chart1M_Count + uint8_t__i) % SIMPLE_CHART_1M_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart1M_Time[uint8_t__idx]);
    }
    file__f.print("]},\"3m\":{\"c\":");
    file__f.print(UINT8_T__G__Chart3M_Count);
    file__f.print(",\"pct\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart3M_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart3M_Index + SIMPLE_CHART_3M_SIZE - UINT8_T__G__Chart3M_Count + uint8_t__i) % SIMPLE_CHART_3M_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart3M_Pct[uint8_t__idx]);
    }
    file__f.print("],\"vin\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart3M_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart3M_Index + SIMPLE_CHART_3M_SIZE - UINT8_T__G__Chart3M_Count + uint8_t__i) % SIMPLE_CHART_3M_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart3M_Vin[uint8_t__idx]);
    }
    file__f.print("],\"vpack\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart3M_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart3M_Index + SIMPLE_CHART_3M_SIZE - UINT8_T__G__Chart3M_Count + uint8_t__i) % SIMPLE_CHART_3M_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart3M_Vpack[uint8_t__idx]);
    }
    file__f.print("],\"time\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart3M_Count; uint8_t__i++)
    {
        uint8_t uint8_t__idx = (uint8_t)((UINT8_T__G__Chart3M_Index + SIMPLE_CHART_3M_SIZE - UINT8_T__G__Chart3M_Count + uint8_t__i) % SIMPLE_CHART_3M_SIZE);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(UINT32_T__G__Chart3M_Time[uint8_t__idx]);
    }
    file__f.print("]}}");
    file__f.close();
}

static void func__Simple_ChartsLoad(void)
{
    if (!BOOL__G__FsOk) return;
    if (!LittleFS.exists(SIMPLE_CHARTS_FILE)) return;
    File file__f = LittleFS.open(SIMPLE_CHARTS_FILE, "r");
    if (!file__f) return;
    String string__content = file__f.readString();
    file__f.close();

    /* برای سادگی، فعلاً فقط 1d را کامل لود می‌کنیم، بقیه را اگر وجود داشت تلاش می‌کنیم */
    /* 1d */
    {
        int32_t int32_t__s = string__content.indexOf("\"1d\":");
        if (int32_t__s >= 0)
        {
            int32_t int32_t__pctS = string__content.indexOf("\"pct\":[", int32_t__s);
            int32_t int32_t__pctE = string__content.indexOf("]", int32_t__pctS);
            int32_t int32_t__vinS = string__content.indexOf("\"vin\":[", int32_t__pctE);
            int32_t int32_t__vinE = string__content.indexOf("]", int32_t__vinS);
            int32_t int32_t__vpackS = string__content.indexOf("\"vpack\":[", int32_t__vinE);
            int32_t int32_t__vpackE = string__content.indexOf("]", int32_t__vpackS);
            int32_t int32_t__timeS = string__content.indexOf("\"time\":[", int32_t__vpackE);
            int32_t int32_t__timeE = string__content.indexOf("]", int32_t__timeS);
            if (int32_t__pctS >=0 && int32_t__vinS >=0 && int32_t__vpackS >=0 && int32_t__timeS >=0)
            {
                String string__pctArr = string__content.substring(int32_t__pctS+7, int32_t__pctE);
                String string__vinArr = string__content.substring(int32_t__vinS+7, int32_t__vinE);
                String string__vpackArr = string__content.substring(int32_t__vpackS+9, int32_t__vpackE);
                String string__timeArr = string__content.substring(int32_t__timeS+8, int32_t__timeE);

                uint8_t uint8_t__count = 0u;
                int32_t int32_t__p = 0;
                while (uint8_t__count < SIMPLE_CHART_1D_SIZE)
                {
                    int32_t int32_t__comma = string__pctArr.indexOf(",", int32_t__p);
                    String string__val;
                    if (int32_t__comma < 0) { string__val = string__pctArr.substring(int32_t__p); }
                    else { string__val = string__pctArr.substring(int32_t__p, int32_t__comma); }
                    string__val.trim();
                    if (string__val.length() == 0u) break;
                    UINT32_T__G__Chart1D_Pct[uint8_t__count] = (uint32_t)string__val.toInt();
                    if (int32_t__comma < 0) { uint8_t__count++; break; }
                    int32_t__p = int32_t__comma + 1;
                    uint8_t__count++;
                }
                uint8_t__count = 0; int32_t__p = 0;
                while (uint8_t__count < SIMPLE_CHART_1D_SIZE)
                {
                    int32_t int32_t__comma = string__vinArr.indexOf(",", int32_t__p);
                    String string__val;
                    if (int32_t__comma < 0) { string__val = string__vinArr.substring(int32_t__p); }
                    else { string__val = string__vinArr.substring(int32_t__p, int32_t__comma); }
                    string__val.trim();
                    if (string__val.length() == 0u) break;
                    UINT32_T__G__Chart1D_Vin[uint8_t__count] = (uint32_t)string__val.toInt();
                    if (int32_t__comma < 0) { uint8_t__count++; break; }
                    int32_t__p = int32_t__comma + 1;
                    uint8_t__count++;
                }
                uint8_t__count = 0; int32_t__p = 0;
                while (uint8_t__count < SIMPLE_CHART_1D_SIZE)
                {
                    int32_t int32_t__comma = string__vpackArr.indexOf(",", int32_t__p);
                    String string__val;
                    if (int32_t__comma < 0) { string__val = string__vpackArr.substring(int32_t__p); }
                    else { string__val = string__vpackArr.substring(int32_t__p, int32_t__comma); }
                    string__val.trim();
                    if (string__val.length() == 0u) break;
                    UINT32_T__G__Chart1D_Vpack[uint8_t__count] = (uint32_t)string__val.toInt();
                    if (int32_t__comma < 0) { uint8_t__count++; break; }
                    int32_t__p = int32_t__comma + 1;
                    uint8_t__count++;
                }
                uint8_t__count = 0; int32_t__p = 0;
                while (uint8_t__count < SIMPLE_CHART_1D_SIZE)
                {
                    int32_t int32_t__comma = string__timeArr.indexOf(",", int32_t__p);
                    String string__val;
                    if (int32_t__comma < 0) { string__val = string__timeArr.substring(int32_t__p); }
                    else { string__val = string__timeArr.substring(int32_t__p, int32_t__comma); }
                    string__val.trim();
                    if (string__val.length() == 0u) break;
                    UINT32_T__G__Chart1D_Time[uint8_t__count] = (uint32_t)string__val.toInt();
                    if (int32_t__comma < 0) { uint8_t__count++; break; }
                    int32_t__p = int32_t__comma + 1;
                    uint8_t__count++;
                }
                UINT8_T__G__Chart1D_Count = uint8_t__count;
                UINT8_T__G__Chart1D_Index = uint8_t__count % SIMPLE_CHART_1D_SIZE;
            }
        }
    }
    /* 7d,1m,3m را ساده لود نمی‌کنیم تا حافظه کم مصرف شود - در صورت نیاز می‌توان مشابه بالا اضافه کرد */
}

/* ==================== پاک‌سازی ==================== */
static void func__Simple_ChargeHistoryClear(void)
{
    UINT8_T__G__ChargeHistoryCount = 0u;
    UINT32_T__G__PrevChargeCountCh1 = UINT32_T__G__ChargeCountCh1;
    UINT32_T__G__PrevChargeCountCh2 = UINT32_T__G__ChargeCountCh2;
    if (BOOL__G__FsOk)
    {
        LittleFS.remove(SIMPLE_CHARGE_HISTORY_FILE);
    }
}

static void func__Simple_ChartsClear(void)
{
    UINT8_T__G__ChartCount = 0u;
    UINT8_T__G__ChartIndex = 0u;
    UINT8_T__G__Chart1D_Count = 0u; UINT8_T__G__Chart1D_Index = 0u;
    UINT8_T__G__Chart7D_Count = 0u; UINT8_T__G__Chart7D_Index = 0u;
    UINT8_T__G__Chart1M_Count = 0u; UINT8_T__G__Chart1M_Index = 0u;
    UINT8_T__G__Chart3M_Count = 0u; UINT8_T__G__Chart3M_Index = 0u;
    UINT32_T__G__Last1D_Ms = 0u; UINT32_T__G__Last7D_Ms = 0u; UINT32_T__G__Last1M_Ms = 0u; UINT32_T__G__Last3M_Ms = 0u;
    if (BOOL__G__FsOk)
    {
        LittleFS.remove(SIMPLE_CHARTS_FILE);
    }
}

static void func__Simple_DiagClear(void)
{
    UINT8_T__G__DiagErrorCount = 0u;
    UINT8_T__G__PowerEventCount = 0u;
}
