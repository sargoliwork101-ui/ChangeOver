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

/* ==================== وب سرور ==================== */
static esp_web_server_t SIMPLE_WEB_SERVER_T__G__Server(SIMPLE_HTTP_PORT);
static char CHAR__G__JsonBuffer[SIMPLE_JSON_BUFFER_SIZE];
static uint32_t UINT32_T__G__LastBrowserPollMs = 0u;
