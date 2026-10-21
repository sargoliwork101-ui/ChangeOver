/* simple_diag.h - مدیریت دیاگ و تاریخچه نمودار با محور زمان - ۱ روزه/۷ روزه/۱ ماهه/۳ ماهه
   فقط فارسی - ذخیره کامل Vin/Vpack/Pct/Time برای همه بازه‌ها */

#include <LittleFS.h>

/* ==================== خواندن آرایهٔ عددی از JSON ====================
   از موقعیت کاراکتر «[» شروع می‌کند و تا «]» عددها را می‌خواند. تعداد
   عددهای خوانده‌شده را برمی‌گرداند؛ آرایهٔ خروجی پر می‌شود. */
static uint8_t func__Simple_JsonReadNumbers(const String &string__content,
                                            int32_t int32_t__startPos,
                                            uint32_t *uint32_t__ptr_out,
                                            uint8_t uint8_t__maxCount)
{
    uint8_t uint8_t__count = 0u;
    int32_t int32_t__pos = int32_t__startPos;
    int32_t int32_t__length = (int32_t)string__content.length();

    if (int32_t__startPos < 0) return 0u;

    /* از موقعیت شروع تا اولین «[» جلو برو (جای شروع می‌تواند روی نام کلید باشد) */
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

/* ==================== ذخیرهٔ پارامترهای خود برد ====================
   پارامترهایی که پنل از برد خوانده (۷۴/۷۵ و چشمک‌ها) روی فایل می‌مانند تا
   بعد از قطع برق، تا رسیدن جواب برد، نمایش با عددهای درست کار کند. */
static void func__Simple_ParamsSave(void)
{
    uint8_t uint8_t__i;

    if (!BOOL__G__FsOk)
    {
        BOOL__G__BoardParamsDirty = false;
        return;
    }

    File file__f = LittleFS.open(SIMPLE_PARAMS_FILE, "w");
    if (!file__f) return;

    file__f.print("{\"pv\":[");
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_PARAM_UI_COUNT; uint8_t__i++)
    {
        uint8_t uint8_t__id = UINT8_T__G__ParamUiIds[uint8_t__i];
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(uint8_t__id);
        file__f.print(",");
        file__f.print(func__Simple_BoardParam(uint8_t__id));
    }
    file__f.print("]}");
    file__f.close();
    BOOL__G__BoardParamsDirty = false;
}

/* ==================== خواندن پارامترهای خود برد از فایل ==================== */
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
    BOOL__G__BoardParamsDirty = false;   /* چیز تازه‌ای نسبت به فایل نیست */
}

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
/* ==================== افزودن یک نقطه به یک بازهٔ نمودار ====================
   قدیمی‌ترین نقطه جای جدیدترین را می‌گیرد (حلقهٔ چرخشی). */
static void func__Simple_ChartPushRange(uint32_t *uint32_t__ptr_pct,
                                        uint32_t *uint32_t__ptr_vin,
                                        uint32_t *uint32_t__ptr_vpack,
                                        uint32_t *uint32_t__ptr_time,
                                        uint8_t *uint8_t__ptr_count, uint8_t *uint8_t__ptr_index,
                                        uint8_t uint8_t__size, uint32_t uint32_t__nowSec,
                                        uint32_t uint32_t__pct, uint32_t uint32_t__vin,
                                        uint32_t uint32_t__vpack)
{
    uint8_t uint8_t__slot = *uint8_t__ptr_index;

    /* زمان هرگز عقب نمی‌افتد: اگر ساعت پنل بعد از یک قطع برق بدون ذخیره عقب
       رفته باشد، نقطه‌ای با زمان قدیمی‌تر از آخرین نقطه ثبت نمی‌شود تا محور
       زمان همیشه صعودی بماند (چند دقیقه بعد خودش جبران می‌شود). */
    if (*uint8_t__ptr_count > 0u)
    {
        uint8_t uint8_t__newestSlot = (uint8_t)((*uint8_t__ptr_index + uint8_t__size - 1u) % uint8_t__size);
        if (uint32_t__nowSec <= uint32_t__ptr_time[uint8_t__newestSlot]) return;
    }

    uint32_t__ptr_pct[uint8_t__slot] = uint32_t__pct;
    uint32_t__ptr_vin[uint8_t__slot] = uint32_t__vin;
    uint32_t__ptr_vpack[uint8_t__slot] = uint32_t__vpack;
    uint32_t__ptr_time[uint8_t__slot] = uint32_t__nowSec;

    *uint8_t__ptr_index = (uint8_t)((uint8_t__slot + 1u) % uint8_t__size);
    if (*uint8_t__ptr_count < uint8_t__size)
    {
        *uint8_t__ptr_count = (uint8_t)(*uint8_t__ptr_count + 1u);
    }
}

/* ==================== افزودن نقطه زمان‌دار به هر چهار بازه ====================
   زمان از «ساعت پنل» می‌آید (فرازمانی که یک‌بار تنظیم و ذخیره شده)؛ تا وقتی
   ساعت تنظیم نشده هیچ نقطه‌ای ثبت نمی‌شود تا محور زمان خراب نشود. */
static void func__Simple_ChartAddTimed(uint32_t uint32_t__pct, uint32_t uint32_t__vin, uint32_t uint32_t__vpack)
{
    uint32_t uint32_t__nowSec = func__Simple_NowSec();
    uint32_t uint32_t__nowMs = (uint32_t)millis();

    if (uint32_t__nowSec == 0u) return;   /* ساعت تنظیم نشده */

    /* ۱ روزه - ۲۴ نقطه: اول هر ۱ دقیقه تا پر شدن، بعد هر ۱ ساعت */
    {
        uint32_t uint32_t__interval = (UINT8_T__G__Chart1D_Count < SIMPLE_CHART_1D_SIZE) ? 60000u : 3600000u;
        if ((uint32_t__nowMs - UINT32_T__G__Last1D_Ms) >= uint32_t__interval)
        {
            func__Simple_ChartPushRange(UINT32_T__G__Chart1D_Pct, UINT32_T__G__Chart1D_Vin,
                                        UINT32_T__G__Chart1D_Vpack, UINT32_T__G__Chart1D_Time,
                                        &UINT8_T__G__Chart1D_Count, &UINT8_T__G__Chart1D_Index,
                                        SIMPLE_CHART_1D_SIZE, uint32_t__nowSec,
                                        uint32_t__pct, uint32_t__vin, uint32_t__vpack);
            UINT32_T__G__Last1D_Ms = uint32_t__nowMs;
        }
    }

    /* ۷ روزه - ۲۸ نقطه: اول هر ۵ دقیقه، بعد هر ۶ ساعت */
    {
        uint32_t uint32_t__interval = (UINT8_T__G__Chart7D_Count < SIMPLE_CHART_7D_SIZE) ? 300000u : 21600000u;
        if ((uint32_t__nowMs - UINT32_T__G__Last7D_Ms) >= uint32_t__interval)
        {
            func__Simple_ChartPushRange(UINT32_T__G__Chart7D_Pct, UINT32_T__G__Chart7D_Vin,
                                        UINT32_T__G__Chart7D_Vpack, UINT32_T__G__Chart7D_Time,
                                        &UINT8_T__G__Chart7D_Count, &UINT8_T__G__Chart7D_Index,
                                        SIMPLE_CHART_7D_SIZE, uint32_t__nowSec,
                                        uint32_t__pct, uint32_t__vin, uint32_t__vpack);
            UINT32_T__G__Last7D_Ms = uint32_t__nowMs;
        }
    }

    /* ۱ ماهه - ۳۰ نقطه: اول هر ۱۵ دقیقه، بعد هر ۱ روز */
    {
        uint32_t uint32_t__interval = (UINT8_T__G__Chart1M_Count < SIMPLE_CHART_1M_SIZE) ? 900000u : 86400000u;
        if ((uint32_t__nowMs - UINT32_T__G__Last1M_Ms) >= uint32_t__interval)
        {
            func__Simple_ChartPushRange(UINT32_T__G__Chart1M_Pct, UINT32_T__G__Chart1M_Vin,
                                        UINT32_T__G__Chart1M_Vpack, UINT32_T__G__Chart1M_Time,
                                        &UINT8_T__G__Chart1M_Count, &UINT8_T__G__Chart1M_Index,
                                        SIMPLE_CHART_1M_SIZE, uint32_t__nowSec,
                                        uint32_t__pct, uint32_t__vin, uint32_t__vpack);
            UINT32_T__G__Last1M_Ms = uint32_t__nowMs;
        }
    }

    /* ۳ ماهه - ۹۰ نقطه: اول هر ۳۰ دقیقه، بعد هر ۱ روز */
    {
        uint32_t uint32_t__interval = (UINT8_T__G__Chart3M_Count < SIMPLE_CHART_3M_SIZE) ? 1800000u : 86400000u;
        if ((uint32_t__nowMs - UINT32_T__G__Last3M_Ms) >= uint32_t__interval)
        {
            func__Simple_ChartPushRange(UINT32_T__G__Chart3M_Pct, UINT32_T__G__Chart3M_Vin,
                                        UINT32_T__G__Chart3M_Vpack, UINT32_T__G__Chart3M_Time,
                                        &UINT8_T__G__Chart3M_Count, &UINT8_T__G__Chart3M_Index,
                                        SIMPLE_CHART_3M_SIZE, uint32_t__nowSec,
                                        uint32_t__pct, uint32_t__vin, uint32_t__vpack);
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

    /* مهر زمان از ساعت پنل؛ اگر ساعت تنظیم نشده باشد صفر می‌ماند و در رابط
       کاربری «زمان نامعلوم» نشان داده می‌شود (زمان الکی ثبت نمی‌کنیم) */
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
    SIMPLE_DIAG_ERROR_T__G__Errors[0].state1 = 0u;
    SIMPLE_DIAG_ERROR_T__G__Errors[0].state2 = 0u;
    if (UINT8_T__G__DiagErrorCount < SIMPLE_DIAG_MAX_ERRORS) UINT8_T__G__DiagErrorCount++;
    BOOL__G__DiagDirty = true;
    BOOL__G__DiagValid = true;
}

static void func__Simple_LocalRecordPower(bool bool__inputPresent)
{
    /* مهر زمان از ساعت پنل؛ اگر ساعت تنظیم نشده باشد صفر می‌ماند و در رابط
       کاربری «زمان نامعلوم» نشان داده می‌شود (زمان الکی ثبت نمی‌کنیم) */
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
        file__f.print(",\"dur\":");
        file__f.print(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i].durationSec);
        file__f.print("}");
    }
    file__f.print("]}");
    file__f.close();
    BOOL__G__ChargeHistDirty = false;
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
            UINT32_T__G__ChargeCountCh1 = UINT32_T__G__PrevChargeCountCh1;
            BOOL__G__ChargeCountBaselineReady = true;
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
            UINT32_T__G__ChargeCountCh2 = UINT32_T__G__PrevChargeCountCh2;
            BOOL__G__ChargeCountBaselineReady = true;
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
        int32_t int32_t__objEnd = string__content.indexOf("}", int32_t__tsEnd);
        if (int32_t__objEnd < 0) int32_t__objEnd = string__content.length();
        String string__ts = string__content.substring(int32_t__tsPos, int32_t__tsEnd);

        int32_t int32_t__chPos = string__content.indexOf("\"ch\":", int32_t__tsEnd);
        if (int32_t__chPos < 0) break;
        int32_t__chPos += 5;
        int32_t int32_t__chEnd = string__content.indexOf(",", int32_t__chPos);
        if (int32_t__chEnd < 0 || int32_t__chEnd > int32_t__objEnd) int32_t__chEnd = int32_t__objEnd;
        String string__ch = string__content.substring(int32_t__chPos, int32_t__chEnd);

        uint32_t uint32_t__duration = 0u;
        int32_t int32_t__durPos = string__content.indexOf("\"dur\":", int32_t__chEnd);
        if (int32_t__durPos >= 0 && int32_t__durPos < int32_t__objEnd)
        {
            int32_t__durPos += 6;
            uint32_t__duration = (uint32_t)string__content.substring(int32_t__durPos, int32_t__objEnd).toInt();
        }

        int32_t__pos = int32_t__objEnd;
        /* مهر زمانی غیرواقعی (ثانیه از بوت در فایل‌های قدیمی) وارد تاریخچه
           نمی‌شود تا محور زمان همه‌جا ثانیهٔ یونیکس باشد */
        if ((uint32_t)string__ts.toInt() < SIMPLE_TIME_VALID_MIN) continue;

        SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__found].timestamp = (uint32_t)string__ts.toInt();
        SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__found].channel = (uint8_t)string__ch.toInt();
        SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__found].durationSec = uint32_t__duration;
        uint8_t__found++;
    }
    UINT8_T__G__ChargeHistoryCount = uint8_t__found;
}

/* ==================== نوشتن یک آرایهٔ نمودار روی فایل ====================
   همیشه از قدیمی‌ترین نقطه به جدیدترین نوشته می‌شود تا ترتیب فایل
   «زمانی» باشد و بازخوانی ساده بماند. */
static void func__Simple_ChartArraySave(File &file__f, const char *char__ptr_name,
                                        const uint32_t *uint32_t__ptr_values,
                                        uint8_t uint8_t__count, uint8_t uint8_t__index,
                                        uint8_t uint8_t__size)
{
    uint8_t uint8_t__i;

    file__f.print("\"");
    file__f.print(char__ptr_name);
    file__f.print("\":[");
    for (uint8_t__i = 0u; uint8_t__i < uint8_t__count; uint8_t__i++)
    {
        uint8_t uint8_t__slot = (uint8_t)((uint8_t__index + uint8_t__size - uint8_t__count + uint8_t__i) % uint8_t__size);
        if (uint8_t__i != 0u) file__f.print(",");
        file__f.print(uint32_t__ptr_values[uint8_t__slot]);
    }
    file__f.print("]");
}

/* ==================== نوشتن یک بازهٔ نمودار ==================== */
static void func__Simple_ChartRangeSave(File &file__f, const char *char__ptr_tag,
                                        const uint32_t *uint32_t__ptr_pct,
                                        const uint32_t *uint32_t__ptr_vin,
                                        const uint32_t *uint32_t__ptr_vpack,
                                        const uint32_t *uint32_t__ptr_time,
                                        uint8_t uint8_t__count, uint8_t uint8_t__index,
                                        uint8_t uint8_t__size)
{
    file__f.print("\"");
    file__f.print(char__ptr_tag);
    file__f.print("\":{\"c\":");
    file__f.print(uint8_t__count);
    file__f.print(",");
    func__Simple_ChartArraySave(file__f, "pct", uint32_t__ptr_pct, uint8_t__count, uint8_t__index, uint8_t__size);
    file__f.print(",");
    func__Simple_ChartArraySave(file__f, "vin", uint32_t__ptr_vin, uint8_t__count, uint8_t__index, uint8_t__size);
    file__f.print(",");
    func__Simple_ChartArraySave(file__f, "vpack", uint32_t__ptr_vpack, uint8_t__count, uint8_t__index, uint8_t__size);
    file__f.print(",");
    func__Simple_ChartArraySave(file__f, "time", uint32_t__ptr_time, uint8_t__count, uint8_t__index, uint8_t__size);
    file__f.print("}");
}

/* ==================== نمودارها فقط در RAM نگه داشته می‌شوند (بدون فرسودگی فلش) ==================== */
static void func__Simple_ChartsSave(void)
{
    BOOL__G__ChartsDirty = false;
}

static void func__Simple_ChartsLoad(void)
{
    BOOL__G__ChartsDirty = false;
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
    BOOL__G__DiagDirty = false;
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
    BOOL__G__ChargeCountBaselineReady = BOOL__G__DiagValid;
    if (BOOL__G__FsOk)
    {
        LittleFS.remove(SIMPLE_CHARGE_HISTORY_FILE);
    }
    if (BOOL__G__ChargeCountBaselineReady)
    {
        func__Simple_ChargeHistorySave();
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
    BOOL__G__DiagClearPending = true;
    if (BOOL__G__FsOk)
    {
        LittleFS.remove(SIMPLE_DIAG_FILE);
    }
    func__Simple_SendDiagReq();
}
