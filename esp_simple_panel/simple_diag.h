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

/* ==================== ثبت محلی خطا - حتی اگر STM32 نفرستد ==================== */
static void func__Simple_LocalRecordFault(uint32_t uint32_t__faultMask, uint32_t uint32_t__vin, uint32_t uint32_t__vpack, uint32_t uint32_t__vLow, uint32_t uint32_t__vHigh)
{
    if (uint32_t__faultMask == 0u) return;
    uint32_t uint32_t__newBits = uint32_t__faultMask & (uint32_t)(~UINT32_T__G__LastLocalFaultMask);
    if (uint32_t__newBits == 0u) return;

    uint32_t uint32_t__nowSec = 0u;
    if (UINT32_T__G__BoardTimeSec != 0u) uint32_t__nowSec = UINT32_T__G__BoardTimeSec;
    else uint32_t__nowSec = (uint32_t)(millis() / 1000u);

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
    SIMPLE_DIAG_ERROR_T__G__Errors[0].state1 = 0u;
    SIMPLE_DIAG_ERROR_T__G__Errors[0].state2 = 0u;
    if (UINT8_T__G__DiagErrorCount < SIMPLE_DIAG_MAX_ERRORS) UINT8_T__G__DiagErrorCount++;
    BOOL__G__DiagValid = true;
}

static void func__Simple_LocalRecordPower(bool bool__inputPresent)
{
    uint32_t uint32_t__nowSec = 0u;
    if (UINT32_T__G__BoardTimeSec != 0u) uint32_t__nowSec = UINT32_T__G__BoardTimeSec;
    else uint32_t__nowSec = (uint32_t)(millis() / 1000u);

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
            BOOL__G__DiagValid = true;
        }
    }
    BOOL__G__LastLocalInputPresent = bool__inputPresent;
}

/* ==================== تیک نمودار هر 5 ثانیه + ثبت محلی خطا/برق ==================== */
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
    uint32_t uint32_t__vpack = uint32_t__vpackRaw; /* برد اصلی درصد را از V24 می‌گیرد - چشمک سبز - نه جمع Vlow+Vhigh */

    /* درصد فول/خالی از پارامترهای خود برد خوانده می‌شود (۷۴/۷۵)، همان‌هایی که
       چراغ سبز برد با آن‌ها چشمک می‌زند؛ و مثل خود برد اول با سقف نگاشت شارژ
       (۱۲۰) گیره می‌شود. */
    uint32_t uint32_t__vMinMv = func__Simple_BoardParam(SIMPLE_PARAM_PCT_VMIN_MV);
    uint32_t uint32_t__vMaxMv = func__Simple_BoardParam(SIMPLE_PARAM_PCT_VMAX_MV);
    uint32_t uint32_t__vMaxChgMv = func__Simple_BoardParam(SIMPLE_PARAM_CHG_PCT_VMAX_MV);
    uint32_t uint32_t__vpackClamped = uint32_t__vpackRaw;
    if (uint32_t__vpackClamped > uint32_t__vMaxChgMv)
    {
        uint32_t__vpackClamped = uint32_t__vMaxChgMv;
    }
    uint32_t uint32_t__pct = 0u;
    if ((uint32_t__vMaxMv > uint32_t__vMinMv) && (uint32_t__vpackClamped >= uint32_t__vMinMv))
    {
        uint32_t__pct = ((uint32_t__vpackClamped - uint32_t__vMinMv) * 100u) / (uint32_t__vMaxMv - uint32_t__vMinMv);
        if (uint32_t__pct > 100u)
        {
            uint32_t__pct = 100u;
        }
    }
    uint32_t uint32_t__imb = UINT32_T__G__TlmField[25];
    func__Simple_ChartAdd(uint32_t__vin, uint32_t__vpack, uint32_t__pct, uint32_t__imb);
    func__Simple_ChartAddTimed(uint32_t__pct, uint32_t__vin, uint32_t__vpack);

    uint32_t uint32_t__faultMask = UINT32_T__G__TlmField[19];
    if (uint32_t__faultMask != 0u)
    {
        func__Simple_LocalRecordFault(uint32_t__faultMask, uint32_t__vin, uint32_t__vpack, uint32_t__vLow, uint32_t__vHigh);
    }
    UINT32_T__G__LastLocalFaultMask = uint32_t__faultMask;

    bool bool__inputPresent = true;
    /* b0 یعنی «قاب معتبر» است نه ورودی؛ ورودی وصل = b1، پس نبودنش یعنی قطع */
    if ((UINT8_T__G__TlmFlags & 0x02u) == 0u) bool__inputPresent = false;
    if (uint32_t__vin < 18000u) bool__inputPresent = false;
    func__Simple_LocalRecordPower(bool__inputPresent);
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

/* ==================== درخواست پارامترهای برد ====================
   درصد فول/خالی باتری و زمان‌بندی چشمک، پارامترهای خود برد هستند؛ این
   درخواست آن‌ها را می‌خواند تا پنل با همان عددهایی حساب کند که چراغ سبز
   برد با آن‌ها چشمک می‌زند. */
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
}

/* ==================== ذخیره دیاگ محلی - خطاها و برق ==================== */
#define SIMPLE_DIAG_FILE            "/diag.json"
static void func__Simple_DiagSave(void)
{
    if (!BOOL__G__FsOk) return;
    File file__f = LittleFS.open(SIMPLE_DIAG_FILE, "w");
    if (!file__f) return;
    file__f.print("{\"ec\":");
    file__f.print(UINT8_T__G__DiagErrorCount);
    file__f.print(",\"pc\":");
    file__f.print(UINT8_T__G__PowerEventCount);
    file__f.print(",\"err\":[");
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__DiagErrorCount; uint8_t__i++)
    {
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print("{\"ts\":");
        file__f.print(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].timestamp);
        file__f.print(",\"fm\":");
        file__f.print(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].faultMask);
        file__f.print(",\"vin\":");
        file__f.print(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vIn);
        file__f.print(",\"vp\":");
        file__f.print(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vPack);
        file__f.print(",\"vl\":");
        file__f.print(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vLow);
        file__f.print(",\"vh\":");
        file__f.print(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vHigh);
        file__f.print("}");
    }
    file__f.print("],\"pwr\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__PowerEventCount; uint8_t__i++)
    {
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print("{\"off\":");
        file__f.print(SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i].offTime);
        file__f.print(",\"on\":");
        file__f.print(SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i].onTime);
        file__f.print(",\"dur\":");
        file__f.print(SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i].durationSec);
        file__f.print("}");
    }
    file__f.print("]}");
    file__f.close();
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
                int32_t int32_t__vhEnd = string__errArr.indexOf("}", int32_t__vh);
                if (int32_t__vhEnd < 0) break;

                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].timestamp = (uint32_t)string__errArr.substring(int32_t__ts, int32_t__tsEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].faultMask = (uint32_t)string__errArr.substring(int32_t__fm, int32_t__fmEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].vIn = (uint32_t)string__errArr.substring(int32_t__vin, int32_t__vinEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].vPack = (uint32_t)string__errArr.substring(int32_t__vp, int32_t__vpEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].vLow = (uint32_t)string__errArr.substring(int32_t__vl, int32_t__vlEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].vHigh = (uint32_t)string__errArr.substring(int32_t__vh, int32_t__vhEnd).toInt();
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].state1 = 0u;
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__cnt].state2 = 0u;
                uint8_t__cnt++;
                int32_t__pos = int32_t__vhEnd + 1;
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
    UINT32_T__G__LastLocalFaultMask = 0u;
    BOOL__G__LastLocalInputValid = false;
    BOOL__G__LocalPowerOffActive = false;
    if (BOOL__G__FsOk)
    {
        LittleFS.remove(SIMPLE_DIAG_FILE);
    }
}
