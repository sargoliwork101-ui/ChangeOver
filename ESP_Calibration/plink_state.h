/* plink_state.h - RX buffers, live TLM data, TX queue, bench window, HTTP server object.
   Included by ESP_Calibration.ino (single translation unit, order matters).
   No include guard on purpose: including twice would redefine everything. */
/* ==================== RX State ==================== */
static esp_rx_state_t ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
static uint8_t  UINT8_T__G__RxType = 0u;
static uint16_t UINT16_T__G__RxLen = 0u;
static uint16_t UINT16_T__G__RxIndex = 0u;
static uint8_t  UINT8_T__G__RxPayload[ESP_LINK_MAX_PAYLOAD];

/* ==================== Live Data ==================== */
static uint32_t UINT32_T__G__TlmField[ESP_LINK_TLM_FIELD_COUNT];

/* [EN] TLM byte 3 since v1.43: imbalance scenario 5 status bits (episode /
 *      latched / output-veto / charge budget spent). Byte 2 stays TlmFlags.
 * [FA] بایت ۳ فریم TLM از نسخهٔ ۱٫۴۳: بیت‌های وضعیت سناریوی ۵. */
static uint8_t UINT8_T__G__TlmFlags2 = 0u;
static uint16_t UINT16_T__G__TlmSeq = 0u;
static uint8_t  UINT8_T__G__TlmFlags = 0u;
static uint32_t UINT32_T__G__TlmFrameCount = 0u;
static uint32_t UINT32_T__G__LastTlmMs = 0u;
static bool     BOOL__G__TlmSeen = false;

static uint32_t UINT32_T__G__ParamApplied[ESP_PARAM_COUNT];
static bool     BOOL__G__ParamKnown[ESP_PARAM_COUNT];

/* ==================== TX Queue (coalesced, rate limited) ==================== */
static uint32_t UINT32_T__G__TxParamValue[ESP_PARAM_COUNT];
static bool     BOOL__G__TxParamPending[ESP_PARAM_COUNT];
static bool     BOOL__G__ParamUserSet[ESP_PARAM_COUNT];

/* ---- ریست دستی قفل‌ها: صف کوچک جدا از جدول پارامترها و پژواک تأیید برد ---- */
static uint8_t  UINT8_T__G__SlotQueueId[ESP_SLOT_QUEUE_LEN];
static uint32_t UINT32_T__G__SlotQueueValue[ESP_SLOT_QUEUE_LEN];
static bool     BOOL__G__SlotQueuePending[ESP_SLOT_QUEUE_LEN];
static uint8_t  UINT8_T__G__SlotLastId = 0u;
static uint32_t UINT32_T__G__SlotLastValue = 0u;
static bool     BOOL__G__SlotLastKnown = false;
static uint32_t UINT32_T__G__SlotRequestCount = 0u;
static uint32_t UINT32_T__G__SlotReportCount = 0u;
static uint32_t UINT32_T__G__SlotNakCount = 0u;
static uint8_t  UINT8_T__G__SlotBusyReject = 0u;
static bool     BOOL__G__TxGetPending = false;
static uint32_t UINT32_T__G__LastTxMs = 0u;
static uint32_t UINT32_T__G__LastKeepaliveMs = 0u;
static uint32_t UINT32_T__G__LastBrowserPollMs = 0u;
static bool     BOOL__G__BrowserSeen = false;
static uint32_t UINT32_T__G__LastParamRefreshMs = 0u;
static bool     BOOL__G__WifiApActive = true;
static bool     BOOL__G__WifiHighPower = true;
static bool     BOOL__G__WifiClientConnected = false;
static bool     BOOL__G__WifiLastReportedConnected = false;
static uint32_t UINT32_T__G__WifiLastReportMs = 0u;
static uint32_t UINT32_T__G__WifiBatteryIdleStartMs = 0u;
static bool     BOOL__G__WifiBatteryIdleTracking = false;
static bool     BOOL__G__WifiBatCutoffLatched = false;
/* [EN] v1.112: when a device joined the AP (0 = none). Gives a newcomer the
        viewer window to open the page before the antenna drops to low power.
   [FA] نسخهٔ ۱٫۱۱۲: لحظهٔ وصل شدن دستگاه به اکسس‌پوینت (۰ = هیچ دستگاهی).
        به تازه‌وارد فرصت بازهٔ تماشاگر را می‌دهد تا پیش از کم شدن توان، پنل
        را باز کند. */
static uint32_t UINT32_T__G__WifiStationSinceMs = 0u;

/* [‎EN] Bench statistics window (see ESP_BENCHLOG_HEADER in plink_config.h) / [FA]‎ پنجرهٔ آمار بنچ (بخش 4 نسخه ۲) */
static bool     BOOL__G__FsOk = false;   /* [EN] LittleFS mounted / [FA] LittleFS سوار شده */
/* [EN] The file system exists but refused to mount. It is NOT formatted
        automatically any more: a format deletes the whole history, which is
        exactly the "nothing was saved" report. The panel offers the button.
   [FA] فایل‌سیستم هست ولی سوار نشد. دیگر خودکار فرمت نمی‌شود: فرمت کل
        تاریخچه را پاک می‌کند و همان «چیزی ذخیره نشده» را می‌سازد. دکمه‌اش در
        پنل است. */
static bool     BOOL__G__FsMountFailed = false;
static uint32_t UINT32_T__G__StatSum[ESP_LINK_TLM_FIELD_COUNT];
static uint32_t UINT32_T__G__StatMin[ESP_LINK_TLM_FIELD_COUNT];
static uint32_t UINT32_T__G__StatMax[ESP_LINK_TLM_FIELD_COUNT];
static uint32_t UINT32_T__G__StatLast[ESP_LINK_TLM_FIELD_COUNT];
static uint32_t UINT32_T__G__StatFaultOr = 0u;
static uint16_t UINT16_T__G__StatSeq = 0u;
static uint8_t  UINT8_T__G__StatFlags = 0u;
static uint32_t UINT32_T__G__StatCount = 0u;

/* [EN] Send priority: charger cut, manual mode, manual duties, then the rest.
   [FA] اولویت ارسال: قطع شارژر، مود دستی، duty دستی، سپس بقیه. */
static const uint8_t UINT8_T__G__TxOrder[ESP_PARAM_COUNT] = { 11, 12, 19, 16, 18, 15, 17, 13, 14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170 };

/* ==================== HTTP ==================== */
static esp_web_server_t ESP_WEB_SERVER_T__G__Server(ESP_HTTP_PORT);
static WiFiUDP WIFI_UDP_T__G__DnsUdp;
static bool BOOL__G__DnsRunning = false;
static char CHAR__G__JsonBuffer[ESP_JSON_BUFFER_SIZE];
