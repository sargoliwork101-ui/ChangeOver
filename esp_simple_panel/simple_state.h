/* simple_state.h - نگهداری تله‌متری زنده پنل ساده
   فقط فارسی - استثنای قانون دو زبانه */

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
static bool BOOL__G__FsOk = false;

/* ==================== تم ==================== */
static uint8_t UINT8_T__G__Theme = 0u; /* 0=تیره اصلی، 1=روشن، 2=شب عمیق */

/* ==================== وب سرور ==================== */
static esp_web_server_t SIMPLE_WEB_SERVER_T__G__Server(SIMPLE_HTTP_PORT);
static char CHAR__G__JsonBuffer[SIMPLE_JSON_BUFFER_SIZE];
static char CHAR__G__SessionIP[24] = {0};
static uint32_t UINT32_T__G__LastBrowserPollMs = 0u;
