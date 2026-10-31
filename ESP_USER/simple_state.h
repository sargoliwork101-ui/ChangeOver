/* simple_state.h - نگهداری تله‌متری زنده و ساعت پنل ساده
   فقط فارسی - استثنای قانون دو زبانه */

#include <LittleFS.h>
/* ==================== وضعیت حافظه ==================== */
static bool BOOL__G__FsOk = false;
/* فایل‌سیستم هست ولی سوار نشد؛ خودکار فرمت نمی‌شود تا تاریخچه پاک نشود */
static bool BOOL__G__FsMountFailed = false;

/* ==================== وضعیت گیرنده ==================== */
static simple_rx_state_t SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_SOF0;
static uint8_t  UINT8_T__G__RxType = 0u;
static uint16_t UINT16_T__G__RxLen = 0u;
static uint16_t UINT16_T__G__RxIndex = 0u;
static uint8_t  UINT8_T__G__RxPayload[SIMPLE_LINK_MAX_PAYLOAD];

/* ==================== داده زنده ==================== */
static uint32_t UINT32_T__G__TlmField[SIMPLE_LINK_TLM_FIELD_COUNT];
static uint8_t  UINT8_T__G__TlmFlags = 0u;
static uint8_t  UINT8_T__G__TlmFlags2 = 0u;
static uint16_t UINT16_T__G__TlmSeq = 0u;
static uint32_t UINT32_T__G__TlmFrameCount = 0u;
static uint32_t UINT32_T__G__LastTlmMs = 0u;
static bool     BOOL__G__TlmSeen = false;

/* ==================== سلامت لینک ==================== */
static uint32_t UINT32_T__G__RxCrcError = 0u;
static uint32_t UINT32_T__G__RxVersionMismatch = 0u;
static uint16_t UINT16_T__G__RxCrc = 0u;
static uint8_t  UINT8_T__G__RxCrcLow = 0u;

/* ==================== دست‌دادن دوطرفه (v1.111) ====================
   وضعیت «آیا برد واقعاً جواب می‌دهد» جدا از «آیا تله‌متری می‌آید» نگه داشته
   می‌شود. مقدارهایی که برد در پاسخ می‌گوید (فریم سالم، خطای CRC، ناهم‌نسخگی)
   همان چیزی است که پنل به کاربر نشان می‌دهد تا مطمئن شود ارتباط «درست» است،
   نه فقط برقرار. */
static bool     BOOL__G__HelloSeen = false;      /* پاسخ تازه از برد داریم */
static uint32_t UINT32_T__G__LastHelloAckMs = 0u; /* زمان آخرین پاسخ */
static uint32_t UINT32_T__G__LastHelloTxMs = 0u;  /* زمان آخرین HELLO فرستاده‌شده */
static uint32_t UINT32_T__G__HelloAckCount = 0u;  /* چند پاسخ گرفتیم */
static uint32_t UINT32_T__G__HelloLossCount = 0u; /* چند بار ارتباط قطع شد */
static uint8_t  UINT8_T__G__BoardProtoVersion = 0u; /* نسخهٔ سیم برد */
static uint8_t  UINT8_T__G__BoardFwVersion = 0u;  /* نسخهٔ فرم‌ور برد */
static uint8_t  UINT8_T__G__BoardFlags = 0u;      /* چه ماژول‌هایی روی برد است */
static uint32_t UINT32_T__G__BoardUptimeSec = 0u; /* آپتایم برد */
static uint32_t UINT32_T__G__BoardRxFrames = 0u;  /* فریم سالم از دید برد */
static uint16_t UINT16_T__G__BoardCrcErrors = 0u; /* خطای CRC از دید برد */
static uint16_t UINT16_T__G__BoardVerErrors = 0u; /* ناهم‌نسخگی از دید برد */
static uint16_t UINT16_T__G__BoardParamCount = 0u;/* تعداد پارامترهای برد */
static uint16_t UINT16_T__G__BoardPeriodMs = 0u;  /* دورهٔ تله‌متری برد */

/* ==================== تاریخچه نمودار - با محور زمان ==================== */
static uint32_t UINT32_T__G__ChartVin[SIMPLE_CHART_HISTORY_SIZE];
static uint32_t UINT32_T__G__ChartVpack[SIMPLE_CHART_HISTORY_SIZE];
static uint32_t UINT32_T__G__ChartPct[SIMPLE_CHART_HISTORY_SIZE];
static uint32_t UINT32_T__G__ChartImb[SIMPLE_CHART_HISTORY_SIZE];
static uint8_t  UINT8_T__G__ChartIndex = 0u;
static uint8_t  UINT8_T__G__ChartCount = 0u;
static uint32_t UINT32_T__G__LastChartMs = 0u;

/* تاریخچه با تفکیک زمانی - محور افقی زمان - درصد + Vin + Vpack برای ۳ ماه */
static uint32_t UINT32_T__G__Chart1D_Pct[SIMPLE_CHART_1D_SIZE];
static uint32_t UINT32_T__G__Chart1D_Vin[SIMPLE_CHART_1D_SIZE];
static uint32_t UINT32_T__G__Chart1D_Vpack[SIMPLE_CHART_1D_SIZE];
static uint32_t UINT32_T__G__Chart1D_Time[SIMPLE_CHART_1D_SIZE];
static uint8_t  UINT8_T__G__Chart1D_Count = 0u;
static uint8_t  UINT8_T__G__Chart1D_Index = 0u;
static uint32_t UINT32_T__G__Last1D_Ms = 0u;

static uint32_t UINT32_T__G__Chart7D_Pct[SIMPLE_CHART_7D_SIZE];
static uint32_t UINT32_T__G__Chart7D_Vin[SIMPLE_CHART_7D_SIZE];
static uint32_t UINT32_T__G__Chart7D_Vpack[SIMPLE_CHART_7D_SIZE];
static uint32_t UINT32_T__G__Chart7D_Time[SIMPLE_CHART_7D_SIZE];
static uint8_t  UINT8_T__G__Chart7D_Count = 0u;
static uint8_t  UINT8_T__G__Chart7D_Index = 0u;
static uint32_t UINT32_T__G__Last7D_Ms = 0u;

static uint32_t UINT32_T__G__Chart1M_Pct[SIMPLE_CHART_1M_SIZE];
static uint32_t UINT32_T__G__Chart1M_Vin[SIMPLE_CHART_1M_SIZE];
static uint32_t UINT32_T__G__Chart1M_Vpack[SIMPLE_CHART_1M_SIZE];
static uint32_t UINT32_T__G__Chart1M_Time[SIMPLE_CHART_1M_SIZE];
static uint8_t  UINT8_T__G__Chart1M_Count = 0u;
static uint8_t  UINT8_T__G__Chart1M_Index = 0u;
static uint32_t UINT32_T__G__Last1M_Ms = 0u;

static uint32_t UINT32_T__G__Chart3M_Pct[SIMPLE_CHART_3M_SIZE];
static uint32_t UINT32_T__G__Chart3M_Vin[SIMPLE_CHART_3M_SIZE];
static uint32_t UINT32_T__G__Chart3M_Vpack[SIMPLE_CHART_3M_SIZE];
static uint32_t UINT32_T__G__Chart3M_Time[SIMPLE_CHART_3M_SIZE];
static uint8_t  UINT8_T__G__Chart3M_Count = 0u;
static uint8_t  UINT8_T__G__Chart3M_Index = 0u;
static uint32_t UINT32_T__G__Last3M_Ms = 0u;

/* ==================== کش دیاگ برد ==================== */
typedef struct
{
    uint32_t timestamp;
    uint32_t faultMask;
    uint32_t vIn;
    uint32_t vPack;
    uint32_t vLow;
    uint32_t vHigh;
    uint32_t state1;
    uint32_t state2;
} simple_diag_error_t;

typedef struct
{
    uint32_t offTime;
    uint32_t onTime;
    uint32_t durationSec;
} simple_power_event_t;

typedef struct
{
    uint32_t timestamp;
    uint8_t channel; /* 1 یا 2 */
    uint32_t durationSec;
} simple_charge_event_t;

static simple_diag_error_t SIMPLE_DIAG_ERROR_T__G__Errors[SIMPLE_DIAG_MAX_ERRORS];
static uint8_t UINT8_T__G__DiagErrorCount = 0u;
static simple_power_event_t SIMPLE_POWER_EVENT_T__G__PowerEvents[SIMPLE_DIAG_MAX_POWER_EVENTS];
static uint8_t UINT8_T__G__PowerEventCount = 0u;
static simple_charge_event_t SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS];
static uint8_t UINT8_T__G__ChargeHistoryCount = 0u;
static uint32_t UINT32_T__G__TotalChargeSec = 0u;
static uint32_t UINT32_T__G__ChargeCount = 0u;
static uint32_t UINT32_T__G__ChargeCountCh1 = 0u;
static uint32_t UINT32_T__G__ChargeCountCh2 = 0u;
static uint32_t UINT32_T__G__ChargeSecCh1 = 0u;
static uint32_t UINT32_T__G__ChargeSecCh2 = 0u;
static uint32_t UINT32_T__G__UptimeSec = 0u;
static uint32_t UINT32_T__G__ResetCount = 0u;
static uint32_t UINT32_T__G__BoardTimeSec = 0u;
static bool BOOL__G__DiagValid = false;
static uint32_t UINT32_T__G__LastDiagReqMs = 0u;

/* ==================== پارامترهای خود برد (درصد باتری و چشمک‌ها) ====================
   درصد فول/خالی باتری از همین پارامترها خوانده می‌شود - همان‌هایی که چراغ سبز
   برد با آن‌ها چشمک می‌زند. تا وقتی برد جواب نداده، پیش‌فرض‌های خودش می‌مانند. */
static uint32_t UINT32_T__G__BoardParam[SIMPLE_PARAM_COUNT];
static bool BOOL__G__BoardParamKnown[SIMPLE_PARAM_COUNT];
static bool BOOL__G__BoardParamSeen = false;
static bool BOOL__G__BoardParamsDirty = false;   /* پارامتر تازه‌ای رسید که باید ذخیره شود */
static uint32_t UINT32_T__G__ParamBulkFrames = 0u;
static uint32_t UINT32_T__G__LastParamsReqMs = 0u;

/* ==================== مقدار پیش‌فرض یک پارامتر (تا آمدن جواب برد) ==================== */
static uint32_t func__Simple_BoardParamDefault(uint8_t uint8_t__id)
{
    switch (uint8_t__id)
    {
        case SIMPLE_PARAM_GREEN_PERIOD_MS:  return 1000u;
        case SIMPLE_PARAM_GREEN_MIN_OFF_MS: return 10u;
        case SIMPLE_PARAM_YELLOW_PERIOD_MS: return 1000u;
        case SIMPLE_PARAM_YELLOW_MIN_ON_MS: return 150u;
        case SIMPLE_PARAM_PCT_VMIN_MV:      return 21000u;
        case SIMPLE_PARAM_PCT_VMAX_MV:      return 29000u;
        case SIMPLE_PARAM_FULL_ENTER_PCT:   return 100u;
        case SIMPLE_PARAM_FULL_EXIT_PCT:    return 95u;
        case SIMPLE_PARAM_CHG_HYST_PCT:     return 5u;
        case SIMPLE_PARAM_RUN_HYST_PCT:     return 2u;
        case SIMPLE_PARAM_RUN_ZERO_EXIT:    return 2u;
        case SIMPLE_PARAM_RUN_ONE_EXIT:     return 3u;
        case SIMPLE_PARAM_CHG_PCT_VMIN_MV:  return 21000u;
        case SIMPLE_PARAM_CHG_PCT_VMAX_MV:  return 29000u;
        case SIMPLE_PARAM_WIFI_IDLE_LOW_POWER_MS: return SIMPLE_WIFI_IDLE_LOW_POWER_MS;
        case SIMPLE_PARAM_WIFI_CYCLE_ON_MS:       return SIMPLE_WIFI_CYCLE_ON_MS;
        case SIMPLE_PARAM_WIFI_CYCLE_OFF_MS:      return SIMPLE_WIFI_CYCLE_OFF_MS;
        case SIMPLE_PARAM_WIFI_BAT_CUTOFF_PCT:    return SIMPLE_WIFI_BAT_CUTOFF_PCT;
        case SIMPLE_PARAM_INPUT_MIN_MV:         return 21000u;
        case SIMPLE_PARAM_INPUT_MAX_MV:         return 28000u;
        case SIMPLE_PARAM_INPUT_HYST_LOW_MV:    return 500u;
        case SIMPLE_PARAM_INPUT_HYST_HIGH_MV:   return 500u;
        case SIMPLE_PARAM_OV_HYST_MV:           return 1000u;
        default:                            return 0u;
    }
}

/* ==================== خواندن یک پارامتر برد (جواب‌داده‌شده یا پیش‌فرض) ==================== */
static uint32_t func__Simple_BoardParam(uint8_t uint8_t__id)
{
    if (uint8_t__id >= SIMPLE_PARAM_COUNT) return 0u;
    if (BOOL__G__BoardParamKnown[uint8_t__id]) return UINT32_T__G__BoardParam[uint8_t__id];
    return func__Simple_BoardParamDefault(uint8_t__id);
}

/* ==================== ریست دستی قفل‌ها (صف و پژواک برد) ====================
   صف چهارخانه‌ای برای درخواست‌های ریست، به‌علاوهٔ پژواک آخرین گزارش برد و
   شمارنده‌های درخواست/تأیید/رد. مقدار اعمال‌شده فقط از پژواک REPORT خوانده
   می‌شود؛ بی‌تأییدی یعنی «هنوز نشسته»، نه «انجام شد». */
static uint8_t  UINT8_T__G__SlotQueueId[SIMPLE_SLOT_QUEUE_LEN];
static uint32_t UINT32_T__G__SlotQueueValue[SIMPLE_SLOT_QUEUE_LEN];
static bool     BOOL__G__SlotQueuePending[SIMPLE_SLOT_QUEUE_LEN];
static uint32_t UINT32_T__G__LastSlotTxMs = 0u;
static uint8_t  UINT8_T__G__SlotLastId = 0u;
static uint32_t UINT32_T__G__SlotLastValue = 0u;
static bool     BOOL__G__SlotLastKnown = false;
static uint32_t UINT32_T__G__SlotRequestCount = 0u;
static uint32_t UINT32_T__G__SlotEchoCount = 0u;
static uint32_t UINT32_T__G__SlotNakCount = 0u;
static uint8_t  UINT8_T__G__SlotNakId = 0u;
static uint8_t  UINT8_T__G__SlotNakCode = 0u;
static uint8_t  UINT8_T__G__SlotBusyReject = 0u;

/* ==================== فهرست پارامترهایی که پنل به آن‌ها نیاز دارد ====================
   ترتیب همین آرایه، ترتیب آرایهٔ pv در JSON است؛ پنل با همین ترتیب می‌خواند. */
static const uint8_t UINT8_T__G__ParamUiIds[SIMPLE_PARAM_UI_COUNT] =
{
    SIMPLE_PARAM_GREEN_PERIOD_MS,   /* 0 */
    SIMPLE_PARAM_GREEN_MIN_OFF_MS,  /* 1 */
    SIMPLE_PARAM_YELLOW_PERIOD_MS,  /* 2 */
    SIMPLE_PARAM_YELLOW_MIN_ON_MS,  /* 3 */
    SIMPLE_PARAM_PCT_VMIN_MV,       /* 4 */
    SIMPLE_PARAM_PCT_VMAX_MV,       /* 5 */
    SIMPLE_PARAM_FULL_ENTER_PCT,    /* 6 */
    SIMPLE_PARAM_FULL_EXIT_PCT,     /* 7 */
    SIMPLE_PARAM_CHG_HYST_PCT,      /* 8 */
    SIMPLE_PARAM_RUN_HYST_PCT,      /* 9 */
    SIMPLE_PARAM_RUN_ZERO_EXIT,     /* 10 */
    SIMPLE_PARAM_RUN_ONE_EXIT,      /* 11 */
    SIMPLE_PARAM_CHG_PCT_VMIN_MV,   /* 12 */
    SIMPLE_PARAM_CHG_PCT_VMAX_MV,   /* 13 */
    SIMPLE_PARAM_INPUT_MIN_MV,      /* 14 */
    SIMPLE_PARAM_INPUT_MAX_MV,      /* 15 */
    SIMPLE_PARAM_INPUT_HYST_LOW_MV, /* 16 */
    SIMPLE_PARAM_INPUT_HYST_HIGH_MV, /* 17 */
    SIMPLE_PARAM_OV_HYST_MV          /* 18 */
};

static bool func__Simple_IsUiParamId(uint8_t uint8_t__id)
{
    uint8_t uint8_t__i;

    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_PARAM_UI_COUNT; uint8_t__i++)
    {
        if (UINT8_T__G__ParamUiIds[uint8_t__i] == uint8_t__id)
        {
            return true;
        }
    }
    return false;
}

/* ==================== ذخیره یک پارامتر رسیده از برد ==================== */
static void func__Simple_StoreBoardParam(uint8_t uint8_t__id, uint32_t uint32_t__value)
{
    uint32_t uint32_t__prevEffective;

    if (uint8_t__id >= SIMPLE_PARAM_COUNT) return;
    uint32_t__prevEffective = func__Simple_BoardParam(uint8_t__id);
    if (func__Simple_IsUiParamId(uint8_t__id) && (uint32_t__prevEffective != uint32_t__value))
    {
        BOOL__G__BoardParamsDirty = true;
    }
    UINT32_T__G__BoardParam[uint8_t__id] = uint32_t__value;
    BOOL__G__BoardParamKnown[uint8_t__id] = true;
    BOOL__G__BoardParamSeen = true;
}

/* ==================== ساعت ====================
   ساعت یک بار از مرورگر گرفته و در فایل ذخیره می‌شود. از آن به بعد لنگر
   نگه داشته می‌شود: زمان = ثانیه ذخیره‌شده + (millis حالا - millis لحظه تنظیم).
   پس ساعت بین فریم‌های دیاگ هم راه می‌رود و بعد از ریست از همان فایل ادامه
   می‌دهد - دیگر هیچ‌وقت پرسیده نمی‌شود. */
static uint32_t UINT32_T__G__TimeAnchorSec = 0u;   /* ثانیه یونیکس در لحظه تنظیم */
static uint32_t UINT32_T__G__TimeAnchorMs = 0u;    /* millis همان لحظه */
static bool BOOL__G__TimeValid = false;            /* ساعت معتبر است (از فایل یا مرورگر) */
static bool BOOL__G__TimePushed = false;           /* ساعت به برد گفته شد */
static uint32_t UINT32_T__G__TimePushMs = 0u;      /* آخرین تلاش برای گفتن ساعت به برد */
static uint32_t UINT32_T__G__TimePushArmedMs = 0u; /* لحظه‌ای که لینک تازه دیده شد (شروع تأخیر) */

/* ==================== ساعت پنل ====================
   تنها منبع «الان» برای همهٔ تاریخچه‌ها. تا وقتی ساعت تنظیم نشده صفر
   برمی‌گرداند تا هیچ نقطه‌ای با زمان الکی (ثانیه از بوت) ذخیره نشود. */
static uint32_t func__Simple_NowSec(void)
{
    uint32_t uint32_t__elapsedSec;
    if (!BOOL__G__TimeValid)
    {
        return 0u;
    }
    uint32_t__elapsedSec = ((uint32_t)millis() - UINT32_T__G__TimeAnchorMs) / 1000u;
    return UINT32_T__G__TimeAnchorSec + uint32_t__elapsedSec;
}

/* ==================== تنظیم ساعت (از فایل، مرورگر یا فریم برد) ==================== */
static void func__Simple_TimeSet(uint32_t uint32_t__epochSec, bool bool__pushToBoard)
{
    if (uint32_t__epochSec < SIMPLE_TIME_VALID_MIN)
    {
        return;
    }
    UINT32_T__G__TimeAnchorSec = uint32_t__epochSec;
    UINT32_T__G__TimeAnchorMs = (uint32_t)millis();
    BOOL__G__TimeValid = true;
    if (bool__pushToBoard)
    {
        BOOL__G__TimePushed = false;   /* در حلقه به برد گفته می‌شود */
    }
}

static uint32_t UINT32_T__G__LastSavedTimeSec = 0u; /* آخرین ساعت ذخیره‌شده روی فلش */
static int32_t  INT32_T__G__LogTzSec = 0;           /* اختلاف افق زمانی گوشی (ثانیه) */
static int32_t  INT32_T__G__LastSavedTzSec = 0;     /* آخرین اختلاف افق زمانی ذخیره‌شده */
static uint32_t UINT32_T__G__LastChartSaveSec = 0u; /* آخرین زمان ذخیرهٔ روزانهٔ نمودارها (ثانیهٔ یونیکس) */

/* ==================== ذخیره ساعت روی فایل (فقط در صورت اختلاف >= ۵ دقیقه یا تغییر افق زمانی) ==================== */
static void func__Simple_TimeSave(void)
{
    uint32_t uint32_t__nowSec;
    uint32_t uint32_t__diff;

    if (!BOOL__G__FsOk || !BOOL__G__TimeValid) return;
    uint32_t__nowSec = func__Simple_NowSec();
    if (uint32_t__nowSec < SIMPLE_TIME_VALID_MIN) return;
    uint32_t__diff = (uint32_t__nowSec >= UINT32_T__G__LastSavedTimeSec)
        ? (uint32_t__nowSec - UINT32_T__G__LastSavedTimeSec)
        : (UINT32_T__G__LastSavedTimeSec - uint32_t__nowSec);
    if ((UINT32_T__G__LastSavedTimeSec != 0u) &&
        (uint32_t__diff < SIMPLE_TIME_ADOPT_DIFF_SEC) &&
        (INT32_T__G__LogTzSec == INT32_T__G__LastSavedTzSec)) return;
    File file__f = LittleFS.open(SIMPLE_TIME_FILE, "w");
    if (!file__f) return;
    file__f.print("{\"ts\":");
    file__f.print((unsigned long)uint32_t__nowSec);
    file__f.print(",\"tz\":");
    file__f.print((long)INT32_T__G__LogTzSec);
    file__f.print("}");
    file__f.close();
    UINT32_T__G__LastSavedTimeSec = uint32_t__nowSec;
    INT32_T__G__LastSavedTzSec = INT32_T__G__LogTzSec;
}

/* ==================== خواندن ساعت از فایل (بعد از هر بوت) ==================== */
static void func__Simple_TimeLoad(void)
{
    uint32_t uint32_t__savedSec;
    int32_t int32_t__tsPos;
    int32_t int32_t__tsEnd;
    int32_t int32_t__commaPos;
    int32_t int32_t__tzPos;

    if (!BOOL__G__FsOk) return;
    if (!LittleFS.exists(SIMPLE_TIME_FILE)) return;

    File file__f = LittleFS.open(SIMPLE_TIME_FILE, "r");
    if (!file__f) return;
    String string__content = file__f.readString();
    file__f.close();

    int32_t__tsPos = string__content.indexOf("\"ts\":");
    if (int32_t__tsPos < 0) return;
    int32_t__tsPos += 5;
    int32_t__tsEnd = string__content.indexOf("}", int32_t__tsPos);
    int32_t__commaPos = string__content.indexOf(",", int32_t__tsPos);
    if ((int32_t__commaPos > int32_t__tsPos) &&
        ((int32_t__tsEnd < 0) || (int32_t__commaPos < int32_t__tsEnd)))
    {
        int32_t__tsEnd = int32_t__commaPos;
    }
    if (int32_t__tsEnd <= int32_t__tsPos) return;
    String string__value = string__content.substring(int32_t__tsPos, int32_t__tsEnd);
    string__value.trim();

    int32_t__tzPos = string__content.indexOf("\"tz\":");
    if (int32_t__tzPos >= 0)
    {
        int32_t int32_t__tzEnd = string__content.indexOf("}", int32_t__tzPos + 5);
        int32_t int32_t__tzComma = string__content.indexOf(",", int32_t__tzPos + 5);
        if ((int32_t__tzComma > (int32_t__tzPos + 5)) &&
            ((int32_t__tzEnd < 0) || (int32_t__tzComma < int32_t__tzEnd)))
        {
            int32_t__tzEnd = int32_t__tzComma;
        }
        if (int32_t__tzEnd > (int32_t__tzPos + 5))
        {
            String string__tzVal = string__content.substring(int32_t__tzPos + 5, int32_t__tzEnd);
            string__tzVal.trim();
            int32_t int32_t__tzParsed = (int32_t)string__tzVal.toInt();
            if ((int32_t__tzParsed >= -50400) && (int32_t__tzParsed <= 50400))
            {
                INT32_T__G__LogTzSec = int32_t__tzParsed;
                INT32_T__G__LastSavedTzSec = int32_t__tzParsed;
            }
        }
    }

    uint32_t__savedSec = (uint32_t)string__value.toInt();
    UINT32_T__G__LastSavedTimeSec = uint32_t__savedSec;
    /* لحظهٔ ذخیره‌شدن روی فایل نه‌فت‌نخورده مانده، پس لنگر را می‌گذاریم
       روی همین ثانیه؛ از این لحظه به بعد با millis جلو می‌رود */
    func__Simple_TimeSet(uint32_t__savedSec, false);
}

/* ==================== پرچم‌های ذخیره ====================
   هر تاریخچه فقط وقتی عوض شود روی فلش نوشته می‌شود (عمر فلش ESP). */
static bool BOOL__G__ChartsDirty = false;
static bool BOOL__G__ChargeHistDirty = false;
static bool BOOL__G__DiagDirty = false;

/* ==================== ردیابی محلی خطا و برق برای ثبت حتی اگر STM32 نفرستد ==================== */
static uint32_t UINT32_T__G__LastLocalFaultMask = 0u;
static bool BOOL__G__LastLocalInputPresent = false;
static bool BOOL__G__LastLocalInputValid = false;
static uint32_t UINT32_T__G__LocalPowerOffTime = 0u;
static bool BOOL__G__LocalPowerOffActive = false;

/* ==================== وای‌فای قابل تنظیم ==================== */
static char CHAR__G__WifiSsid[SIMPLE_WIFI_SSID_MAX + 1] = SIMPLE_WIFI_AP_SSID;
static char CHAR__G__WifiPass[SIMPLE_WIFI_PASS_MAX + 1] = SIMPLE_WIFI_AP_PASS;
static bool BOOL__G__WifiRebootPending = false;
static uint32_t UINT32_T__G__WifiRebootMs = 0u;

/* ==================== ذخیره prev شارژ برای ریستارت ==================== */
static uint32_t UINT32_T__G__PrevChargeCountCh1 = 0u;
static uint32_t UINT32_T__G__PrevChargeCountCh2 = 0u;
static bool BOOL__G__ChargeCountBaselineReady = false;
static bool BOOL__G__DiagClearPending = false;

/* ==================== کاربر و سشن ==================== */
typedef struct
{
    char name[SIMPLE_USER_NAME_MAX + 1];
    char passHash[65];
    uint8_t role; /* 0=viewer, 1=admin */
    bool active;
} simple_user_t;

static simple_user_t SIMPLE_USER_T__G__Users[SIMPLE_USERS_MAX];
static uint8_t UINT8_T__G__UserCount = 0u;
static char CHAR__G__CurrentUser[SIMPLE_USER_NAME_MAX + 1] = {0};
static uint8_t UINT8_T__G__CurrentRole = 0u;
static uint32_t UINT32_T__G__SessionStartMs = 0u;
static char CHAR__G__SessionToken[SIMPLE_SESSION_TOKEN_LEN + 1u] = {0};

/* ==================== بازیابی رمز ادمین ====================
   SessionHadLogin: در همین بوت کسی وارد شده (پرچم بازیابی پاک می‌شود)
   RecoveryArmed: پرچم «بوت بدون ورود» همین حالا روی فلش نوشته شده
   RecoveryHappened: در همین بوت کاربران به پیش‌فرض برگشتند (برای پیغام صفحهٔ ورود) */
static bool BOOL__G__SessionHadLogin = false;
static bool BOOL__G__RecoveryArmed = false;
static bool BOOL__G__RecoveryHappened = false;
static uint32_t UINT32_T__G__LastRecoveryTryMs = 0u;

/* ==================== تم ==================== */
static uint8_t UINT8_T__G__Theme = 0u; /* 0=تیره، 1=روشن - تم سوم حذف شد */

/* ==================== کلید رمزنگاری فایل‌های حساس ====================
   از شناسهٔ یکتای چیپ ساخته می‌شود؛ روی فلش ذخیره نمی‌شود و از بیرون
   خوانده نمی‌شود. تا وقتی ساخته نشده، فایل‌های رمزشده باز نمی‌شوند. */
static uint8_t UINT8_T__G__CryptoKey[SIMPLE_CRYPTO_KEY_LEN];
static bool BOOL__G__CryptoKeyReady = false;
static uint32_t UINT32_T__G__CryptoNonce = 0u;   /* شمارندهٔ یکتای هر نوشتن */

/* ==================== وب سرور ==================== */
static esp_web_server_t SIMPLE_WEB_SERVER_T__G__Server(SIMPLE_HTTP_PORT);
static WiFiUDP WIFI_UDP_T__G__DnsUdp;
static bool BOOL__G__DnsRunning = false;
static char CHAR__G__JsonBuffer[SIMPLE_JSON_BUFFER_SIZE];
static uint32_t UINT32_T__G__LastBrowserPollMs = 0u;
static bool BOOL__G__BrowserSeen = false;

/* ==================== وضعیت کاهش مصرف و قدرت آنتن وای‌فای ==================== */
static bool BOOL__G__WifiApActive = true;
static bool BOOL__G__WifiHighPower = true;
static bool BOOL__G__WifiClientConnected = false;
static bool BOOL__G__WifiLastReportedConnected = false;
static uint32_t UINT32_T__G__WifiLastReportMs = 0u;
static uint32_t UINT32_T__G__WifiBatteryIdleStartMs = 0u;
static bool BOOL__G__WifiBatteryIdleTracking = false;
static bool BOOL__G__WifiBatCutoffLatched = false;
/* v1.112: لحظه‌ای که یک دستگاه به اکسس‌پوینت وصل شد (۰ = هیچ دستگاهی وصل نیست).
   برای این‌که کاربری که تازه وصل شده فرصت باز کردن پنل را داشته باشد، تا
   ‎SIMPLE_WIFI_VIEWER_HOLD_MS‎ توان آنتن روی بیشینه می‌ماند. */
static uint32_t UINT32_T__G__WifiStationSinceMs = 0u;
