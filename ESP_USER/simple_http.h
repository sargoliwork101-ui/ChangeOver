#include "about_content.h"
#include "about_footer.h"
#include "charge_calendar.h"
static bool func__Simple_ParseInt(const char *char__ptr_text, int32_t *int32_t__ptr_value)
{
    bool bool__negative = (char__ptr_text[0] == '-');
    uint8_t uint8_t__index = bool__negative ? 1u : 0u;
    uint8_t uint8_t__digits = 0u;
    int64_t int64_t__value = 0;
    while (char__ptr_text[uint8_t__index] != '\0')
    {
        char char__digit = char__ptr_text[uint8_t__index];
        if ((char__digit < '0') || (char__digit > '9') || (uint8_t__digits >= 10u))
        {
            return false;
        }
        int64_t__value = int64_t__value * 10 + (int64_t)(char__digit - '0');
        uint8_t__digits++;
        uint8_t__index++;
    }
    if (uint8_t__digits == 0u) return false;
    if (bool__negative)
    {
        if (int64_t__value > 2147483648LL) return false;
        *int32_t__ptr_value = (int32_t)(-int64_t__value);
    }
    else
    {
        if (int64_t__value > 2147483647LL) return false;
        *int32_t__ptr_value = (int32_t)int64_t__value;
    }
    return true;
}
/* ===== JSON - plain body ===== */
static bool func__Simple_JsonGetString(const String &string__json, const char *char__ptr_key, String &string__out)
{
    String string__search = String("\"") + char__ptr_key + String("\"");
    int int__pos = string__json.indexOf(string__search);
    if (int__pos < 0) return false;
    int__pos = string__json.indexOf(":", int__pos + (int)string__search.length());
    if (int__pos < 0) return false;
    int__pos++;
    while (int__pos < (int)string__json.length() && (string__json[int__pos] == ' ' || string__json[int__pos] == '\t' || string__json[int__pos] == '\r' || string__json[int__pos] == '\n')) int__pos++;
    if (int__pos >= (int)string__json.length()) return false;
    if (string__json[int__pos] == '"')
    {
        int__pos++;
        int int__end = string__json.indexOf("\"", int__pos);
        if (int__end < 0) return false;
        string__out = string__json.substring(int__pos, int__end);
        return true;
    }
    else
    {
        int int__end = int__pos;
        while (int__end < (int)string__json.length() && string__json[int__end] != ',' && string__json[int__end] != '}' && string__json[int__end] != ' ' && string__json[int__end] != '\r' && string__json[int__end] != '\n') int__end++;
        string__out = string__json.substring(int__pos, int__end);
        string__out.trim();
        return string__out.length() > 0;
    }
}
static bool func__Simple_GetArg(const char *char__ptr_name, String &string__out)
{
    if (SIMPLE_WEB_SERVER_T__G__Server.hasArg(char__ptr_name))
    {
        string__out = SIMPLE_WEB_SERVER_T__G__Server.arg(char__ptr_name);
        return true;
    }
    if (SIMPLE_WEB_SERVER_T__G__Server.hasArg("plain"))
    {
        String string__plain = SIMPLE_WEB_SERVER_T__G__Server.arg("plain");
        if (string__plain.length() > 0)
        {
            String string__val;
            if (func__Simple_JsonGetString(string__plain, char__ptr_name, string__val))
            {
                string__out = string__val;
                return true;
            }
        }
    }
    return false;
}
/*
 * [EN] Keep the JSON whole: a piece that does not fit is dropped instead of
 * being cut in half, because half a number makes the panel show nothing.
 */
static bool func__Simple_JsonPut(char *char__ptr_buf, size_t size_t__cap,
                                 size_t *size_t__ptrUsed, int int__written)
{
    if ((char__ptr_buf == NULL) || (size_t__ptrUsed == NULL) || (int__written < 0))
    {
        return false;
    }
    if ((size_t)int__written >= (size_t__cap - *size_t__ptrUsed))
    {
        return false;
    }
    *size_t__ptrUsed += (size_t)int__written;
    return true;
}
/* ==================== Compiled About content ==================== */
/** @brief Serve editable company JSON from program memory; no board commands. */
static void func__Simple_HttpAbout(void)
{
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(sizeof(SIMPLE_ABOUT_JSON) - 1u);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json; charset=utf-8", "");
    SIMPLE_WEB_SERVER_T__G__Server.sendContent_P(SIMPLE_ABOUT_JSON, sizeof(SIMPLE_ABOUT_JSON) - 1u);
}
/* ==================== Company footer browser asset ==================== */
/** @brief Stream the footer asset directly from flash without a large RAM allocation. */
static void func__Simple_HttpAboutScript(void)
{
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(sizeof(SIMPLE_ABOUT_FOOTER_JS) - 1u);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/javascript; charset=utf-8", "");
    for (size_t offset = 0u; offset < sizeof(SIMPLE_ABOUT_FOOTER_JS) - 1u; offset += 1024u)
    {
        size_t remaining = sizeof(SIMPLE_ABOUT_FOOTER_JS) - 1u - offset;
        SIMPLE_WEB_SERVER_T__G__Server.sendContent_P(SIMPLE_ABOUT_FOOTER_JS + offset, remaining < 1024u ? remaining : 1024u);
        func__Simple_DrainSerial();
        yield();
    }
}
/* ==================== Calendar browser asset ==================== */
/** @brief Stream the shared browser asset from flash without a large RAM copy. */
static void func__Simple_HttpCalendar(void)
{
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(sizeof(SIMPLE_CHARGE_CALENDAR_JS) - 1u);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/javascript; charset=utf-8", "");
    for (size_t offset = 0u; offset < sizeof(SIMPLE_CHARGE_CALENDAR_JS) - 1u; offset += 1024u)
    {
        size_t remaining = sizeof(SIMPLE_CHARGE_CALENDAR_JS) - 1u - offset;
        SIMPLE_WEB_SERVER_T__G__Server.sendContent_P(SIMPLE_CHARGE_CALENDAR_JS + offset, remaining < 1024u ? remaining : 1024u);
        func__Simple_DrainSerial();
        yield();
    }
}
static void func__Simple_HttpRoot(void)
{
    uint32_t uint32_t__startMs = (uint32_t)millis();
    BOOL__G__BrowserSeen = true;
    UINT32_T__G__LastBrowserPollMs = uint32_t__startMs;
#if defined(ESP8266)
    SIMPLE_WEB_SERVER_T__G__Server.keepAlive(false);
#endif
    SIMPLE_WEB_SERVER_T__G__Server.client().setTimeout(SIMPLE_HTTP_SEND_TIMEOUT_MS);
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Connection", "close");
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/html", "");
    static const size_t SIZE_T__CHUNK = 2048u;
    const size_t SIZE_T__BYTES = sizeof(SIMPLE_PANEL_HTML) - 1u;
    for (size_t SIZE_T__OFFSET = 0u; SIZE_T__OFFSET < SIZE_T__BYTES; SIZE_T__OFFSET += SIZE_T__CHUNK)
    {
        if (SIMPLE_WEB_SERVER_T__G__Server.client().connected() == false)
        {
            break;
        }
        if ((uint32_t)((uint32_t)millis() - uint32_t__startMs) > SIMPLE_HTTP_ROOT_MAX_MS)
        {
            SIMPLE_WEB_SERVER_T__G__Server.client().stop();
            break;
        }
        const size_t SIZE_T__REMAINING = SIZE_T__BYTES - SIZE_T__OFFSET;
        const size_t SIZE_T__COUNT = (SIZE_T__REMAINING < SIZE_T__CHUNK) ? SIZE_T__REMAINING : SIZE_T__CHUNK;
        SIMPLE_WEB_SERVER_T__G__Server.sendContent_P(SIMPLE_PANEL_HTML + SIZE_T__OFFSET, SIZE_T__COUNT);
        func__Simple_DrainSerial();
#if defined(ESP8266) || defined(ESP32)
        yield();
#endif
    }
}
/* ==================== - boardTime ==================== */
static void func__Simple_HttpTelemetry(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__ageMs = uint32_t__nowMs - UINT32_T__G__LastTlmMs;
    uint32_t uint32_t__panelTimeSec = func__Simple_TimeBootReady() ? func__Simple_NowSec() : 0u;
    bool bool__online = BOOL__G__TlmSeen && (uint32_t__ageMs <= SIMPLE_LINK_TIMEOUT_MS);
    bool bool__authed = func__Simple_SessionValid();
    const char *char__ptr_user = bool__authed ? CHAR__G__CurrentUser : "";
    uint8_t uint8_t__role = bool__authed ? UINT8_T__G__CurrentRole : 0u;
    uint8_t uint8_t__index;
    size_t size_t__used;
    /* [EN] Three bytes stay free for the closing ]}  so the reply is always
       parseable, even when every field carries its widest value. */
    const size_t size_t__cap = (size_t)SIMPLE_JSON_BUFFER_SIZE - 3u;
    BOOL__G__BrowserSeen = true;
    UINT32_T__G__LastBrowserPollMs = uint32_t__nowMs;
    func__Simple_ChartTick();
    {
        int int__head = snprintf(CHAR__G__JsonBuffer, SIMPLE_JSON_BUFFER_SIZE,
            "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"fl2\":%u,\"n\":%lu,\"ce\":%lu,\"vm\":%lu,\"inputPresent\":%u,\"user\":\"%s\",\"role\":%u,\"theme\":%u,\"boardTime\":%lu,\"bt\":%lu,\"timeOk\":%u,\"pvOk\":%u,\"recovery\":%u,"
            "\"lko\":%u,\"bpv\":%u,\"bfw\":%u,\"bfl\":%u,\"brx\":%lu,\"bce\":%u,\"bvm\":%u,\"bpc\":%u,\"bpr\":%u,\"hac\":%lu,\"hls\":%lu,\"lac\":%lu,\"t\":[",
            bool__online ? 1u : 0u,
            (unsigned long)uint32_t__ageMs,
            (unsigned int)UINT16_T__G__TlmSeq,
            (unsigned int)UINT8_T__G__TlmFlags,
            (unsigned int)UINT8_T__G__TlmFlags2,
            (unsigned long)UINT32_T__G__TlmFrameCount,
            (unsigned long)UINT32_T__G__RxCrcError,
            (unsigned long)UINT32_T__G__RxVersionMismatch,
            (UINT8_T__G__TlmFlags & 0x02u) ? 1u : 0u,
            char__ptr_user,
            (unsigned int)uint8_t__role,
            (unsigned int)UINT8_T__G__Theme,
            (unsigned long)uint32_t__panelTimeSec,
            (unsigned long)uint32_t__panelTimeSec,
            func__Simple_TimeBootReady() ? 1u : 0u,
            BOOL__G__BoardParamSeen ? 1u : 0u,
            0u,
            BOOL__G__HelloSeen ? 1u : 0u,
            (unsigned int)UINT8_T__G__BoardProtoVersion,
            (unsigned int)UINT8_T__G__BoardFwVersion,
            (unsigned int)UINT8_T__G__BoardFlags,
            (unsigned long)UINT32_T__G__BoardRxFrames,
            (unsigned int)UINT16_T__G__BoardCrcErrors,
            (unsigned int)UINT16_T__G__BoardVerErrors,
            (unsigned int)UINT16_T__G__BoardParamCount,
            (unsigned int)UINT16_T__G__BoardPeriodMs,
            (unsigned long)UINT32_T__G__HelloAckCount,
            (unsigned long)UINT32_T__G__HelloLossCount,
            (unsigned long)(BOOL__G__HelloSeen ? (uint32_t__nowMs - UINT32_T__G__LastHelloAckMs)
                                               : 0xFFFFFFFFu));
        if ((int__head <= 0) || ((size_t)int__head >= size_t__cap))
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(500, "application/json", "{\"ok\":0,\"e\":\"json\"}");
            return;
        }
        size_t__used = (size_t)int__head;
    }
    for (uint8_t__index = 0u; uint8_t__index < SIMPLE_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
        int int__written = snprintf(&CHAR__G__JsonBuffer[size_t__used],
            size_t__cap - size_t__used,
            "%s%lu", char__ptr_sep, (unsigned long)UINT32_T__G__TlmField[uint8_t__index]);
        if (!func__Simple_JsonPut(CHAR__G__JsonBuffer, size_t__cap, &size_t__used, int__written))
        {
            break;
        }
    }
    if (func__Simple_JsonPut(CHAR__G__JsonBuffer, size_t__cap, &size_t__used,
                             snprintf(&CHAR__G__JsonBuffer[size_t__used],
                                      size_t__cap - size_t__used, "],\"pv\":[")))
    {
        for (uint8_t__index = 0u; uint8_t__index < SIMPLE_PARAM_UI_COUNT; uint8_t__index++)
        {
            const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
            int int__written = snprintf(&CHAR__G__JsonBuffer[size_t__used],
                size_t__cap - size_t__used,
                "%s%lu", char__ptr_sep,
                (unsigned long)func__Simple_BoardParam(UINT8_T__G__ParamUiIds[uint8_t__index]));
            if (!func__Simple_JsonPut(CHAR__G__JsonBuffer, size_t__cap, &size_t__used, int__written))
            {
                break;
            }
        }
        (void)func__Simple_JsonPut(CHAR__G__JsonBuffer, size_t__cap, &size_t__used,
                                   snprintf(&CHAR__G__JsonBuffer[size_t__used],
                                            size_t__cap - size_t__used, "]"));
    }
    (void)snprintf(&CHAR__G__JsonBuffer[size_t__used],
                   (size_t)SIMPLE_JSON_BUFFER_SIZE - size_t__used, "}");
#if defined(ESP8266)
    SIMPLE_WEB_SERVER_T__G__Server.keepAlive(false);
#endif
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Connection", "close");
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}
/* [EN] One array, gathered into a small buffer: four numbers per TCP write
 * instead of one, so a 3-month chart costs ~20 sends, not 360. */
static void func__Simple_SendChartArray(const char *char__ptr_head,
                                        const uint32_t *uint32_t__ptr_values,
                                        uint16_t uint16_t__size, uint16_t uint16_t__count,
                                        uint16_t uint16_t__index)
{
    char char__buf[96];
    size_t size_t__used = 0u;
    uint16_t uint16_t__i;
    SIMPLE_WEB_SERVER_T__G__Server.sendContent(char__ptr_head);
    for (uint16_t__i = 0u; uint16_t__i < uint16_t__count; uint16_t__i++)
    {
        int int__slot = (int)uint16_t__index + (int)uint16_t__size - (int)uint16_t__count + (int)uint16_t__i;
        int int__written;
        while (int__slot >= (int)uint16_t__size)
        {
            int__slot -= (int)uint16_t__size;
        }
        int__written = snprintf(&char__buf[size_t__used], sizeof(char__buf) - size_t__used,
                                (uint16_t__i == 0u) ? "%lu" : ",%lu",
                                (unsigned long)uint32_t__ptr_values[int__slot]);
        if ((int__written < 0) || ((size_t)int__written >= (sizeof(char__buf) - size_t__used)))
        {
            break;
        }
        size_t__used += (size_t)int__written;
        if ((sizeof(char__buf) - size_t__used) < 14u)
        {
            char__buf[size_t__used] = '\0';
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(char__buf);
            size_t__used = 0u;
        }
    }
    if (size_t__used > 0u)
    {
        char__buf[size_t__used] = '\0';
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(char__buf);
    }
}
static void func__Simple_HttpCharts(void)
{
    String string__range = SIMPLE_WEB_SERVER_T__G__Server.hasArg("range") ? SIMPLE_WEB_SERVER_T__G__Server.arg("range") : "1d";
    const char *char__ptr_tag = "1d";
    uint16_t uint16_t__size = (uint16_t)SIMPLE_CHART_1D_SIZE;
    uint16_t uint16_t__count = UINT16_T__G__Chart1D_Count;
    uint16_t uint16_t__index = UINT16_T__G__Chart1D_Index;
    const uint32_t *uint32_t__ptrPct = UINT32_T__G__Chart1D_Pct;
    const uint32_t *uint32_t__ptrVin = UINT32_T__G__Chart1D_Vin;
    const uint32_t *uint32_t__ptrVpack = UINT32_T__G__Chart1D_Vpack;
    const uint32_t *uint32_t__ptrTime = UINT32_T__G__Chart1D_Time;
    char char__head[48];
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    if (string__range == "7d")
    {
        char__ptr_tag = "7d";
        uint16_t__size = (uint16_t)SIMPLE_CHART_7D_SIZE;
        uint16_t__count = UINT16_T__G__Chart7D_Count;
        uint16_t__index = UINT16_T__G__Chart7D_Index;
        uint32_t__ptrPct = UINT32_T__G__Chart7D_Pct;
        uint32_t__ptrVin = UINT32_T__G__Chart7D_Vin;
        uint32_t__ptrVpack = UINT32_T__G__Chart7D_Vpack;
        uint32_t__ptrTime = UINT32_T__G__Chart7D_Time;
    }
    else if (string__range == "1m")
    {
        char__ptr_tag = "1m";
        uint16_t__size = (uint16_t)SIMPLE_CHART_1M_SIZE;
        uint16_t__count = UINT16_T__G__Chart1M_Count;
        uint16_t__index = UINT16_T__G__Chart1M_Index;
        uint32_t__ptrPct = UINT32_T__G__Chart1M_Pct;
        uint32_t__ptrVin = UINT32_T__G__Chart1M_Vin;
        uint32_t__ptrVpack = UINT32_T__G__Chart1M_Vpack;
        uint32_t__ptrTime = UINT32_T__G__Chart1M_Time;
    }
    else if (string__range == "3m")
    {
        char__ptr_tag = "3m";
        uint16_t__size = (uint16_t)SIMPLE_CHART_3M_SIZE;
        uint16_t__count = UINT16_T__G__Chart3M_Count;
        uint16_t__index = UINT16_T__G__Chart3M_Index;
        uint32_t__ptrPct = UINT32_T__G__Chart3M_Pct;
        uint32_t__ptrVin = UINT32_T__G__Chart3M_Vin;
        uint32_t__ptrVpack = UINT32_T__G__Chart3M_Vpack;
        uint32_t__ptrTime = UINT32_T__G__Chart3M_Time;
    }
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "");
    (void)snprintf(char__head, sizeof(char__head), "{\"range\":\"%s\",\"count\":%u,\"pct\":[",
                   char__ptr_tag, (unsigned int)uint16_t__count);
    func__Simple_SendChartArray(char__head, uint32_t__ptrPct, uint16_t__size, uint16_t__count, uint16_t__index);
    func__Simple_SendChartArray("],\"vin\":[", uint32_t__ptrVin, uint16_t__size, uint16_t__count, uint16_t__index);
    func__Simple_SendChartArray("],\"vpack\":[", uint32_t__ptrVpack, uint16_t__size, uint16_t__count, uint16_t__index);
    func__Simple_SendChartArray("],\"time\":[", uint32_t__ptrTime, uint16_t__size, uint16_t__count, uint16_t__index);
    SIMPLE_WEB_SERVER_T__G__Server.sendContent("]}");
}
static void func__Simple_HttpDiag(void)
{
    uint32_t uint32_t__now = (uint32_t)millis();
    if ((uint32_t__now - UINT32_T__G__LastDiagReqMs) > 5000u)
    {
        func__Simple_SendDiagReq();
    }
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "");
    String s; s.reserve(384);
    s = "{\"valid\":"; s += BOOL__G__DiagValid ? "1" : "0";
    s += ",\"totalChargeSec\":"; s += String(UINT32_T__G__TotalChargeSec);
    s += ",\"chargeCount\":"; s += String(UINT32_T__G__ChargeCount);
    s += ",\"chargeCountCh1\":"; s += String(UINT32_T__G__ChargeCountCh1);
    s += ",\"chargeCountCh2\":"; s += String(UINT32_T__G__ChargeCountCh2);
    s += ",\"chargeSecCh1\":"; s += String(UINT32_T__G__ChargeSecCh1);
    s += ",\"chargeSecCh2\":"; s += String(UINT32_T__G__ChargeSecCh2);
    s += ",\"uptimeSec\":"; s += String(UINT32_T__G__UptimeSec);
    s += ",\"resetCount\":"; s += String(UINT32_T__G__ResetCount);
    s += ",\"boardTime\":"; s += String(UINT32_T__G__BoardTimeSec);
    s += ",\"panelTime\":"; s += String(func__Simple_TimeBootReady() ? func__Simple_NowSec() : 0u);
    s += ",\"timeOk\":"; s += String(func__Simple_TimeBootReady() ? 1u : 0u);
    s += ",\"errCount\":"; s += String(UINT8_T__G__DiagErrorCount);
    s += ",\"powerCount\":"; s += String(UINT8_T__G__PowerEventCount);
    s += ",\"errors\":[";
    SIMPLE_WEB_SERVER_T__G__Server.sendContent(s);
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__DiagErrorCount; uint8_t__i++)
    {
        if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
        String e; e.reserve(128);
        e = "{\"ts\":"; e += String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].timestamp);
        e += ",\"fault\":"; e += String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].faultMask);
        e += ",\"vin\":"; e += String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vIn);
        e += ",\"vpack\":"; e += String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vPack);
        e += ",\"vlow\":"; e += String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vLow);
        e += ",\"vhigh\":"; e += String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].vHigh);
        e += ",\"s1\":"; e += String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].state1);
        e += ",\"s2\":"; e += String(SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__i].state2);
        e += "}";
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(e);
    }
    SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"power\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__PowerEventCount; uint8_t__i++)
    {
        if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
        String p; p.reserve(64);
        p = "{\"off\":"; p += String(SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i].offTime);
        p += ",\"on\":"; p += String(SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i].onTime);
        p += ",\"dur\":"; p += String(SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__i].durationSec);
        p += "}";
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(p);
    }
    SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"chargeHistory\":[");
    for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__ChargeHistoryCount; uint8_t__i++)
    {
        if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
        String ch; ch.reserve(48);
        ch = "{\"ts\":"; ch += String(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i].timestamp);
        ch += ",\"ch\":"; ch += String(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i].channel);
        ch += ",\"dur\":"; ch += String(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i].durationSec);
        ch += ",\"start\":"; ch += String(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i].startSec);
        uint8_t status = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i].status;
        ch += ",\"status\":\"";
        ch += status == 1u ? "complete" : status == 2u ? "partial" : status == 3u ? "charging" : "unknown";
        ch += "\"}";
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(ch);
    }
    SIMPLE_WEB_SERVER_T__G__Server.sendContent("]}");
}
/* ==================== - JSON + query ==================== */
static void func__Simple_HttpTimeSync(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__tsStr;
    int32_t int32_t__ts = 0;
    bool bool__found = false;
    if (func__Simple_GetArg("ts", string__tsStr))
    {
        if (func__Simple_ParseInt(string__tsStr.c_str(), &int32_t__ts) && int32_t__ts > 0) bool__found = true;
    }
    if (!bool__found)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"ts\"}");
        return;
    }
    String string__tz;
    if (func__Simple_GetArg("tz", string__tz))
    {
        int32_t int32_t__tz = 0;
        if (func__Simple_ParseInt(string__tz.c_str(), &int32_t__tz) &&
            (int32_t__tz >= -50400) && (int32_t__tz <= 50400))
        {
            INT32_T__G__LogTzSec = int32_t__tz;
        }
    }
    func__Simple_TimeSet((uint32_t)int32_t__ts, true);
    func__Simple_TimeSave();
    func__Simple_SendTimeSync(func__Simple_NowSec());
    UINT32_T__G__BoardTimeSec = func__Simple_NowSec();
    BOOL__G__TimePushed = true;
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}
/* ==================== - JSON + form ==================== */
static void func__Simple_HttpLogin(void)
{
    String string__user, string__pass;
    bool bool__hasUser = func__Simple_GetArg("user", string__user);
    bool bool__hasPass = func__Simple_GetArg("pass", string__pass);
    if (!bool__hasUser || !bool__hasPass)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }
    int32_t int32_t__login = func__Simple_AuthLogin(string__user.c_str(), string__pass.c_str());
    if (int32_t__login == 1)
    {
        char char__buf[192];
        char char__cookie[160];
        (void)snprintf(char__cookie, sizeof(char__cookie),
                       "%s=%s; Path=/; HttpOnly; SameSite=Strict",
                       SIMPLE_SESSION_COOKIE, CHAR__G__SessionToken);
        SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Set-Cookie", char__cookie);
        SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
        (void)snprintf(char__buf, sizeof(char__buf), "{\"ok\":1,\"user\":\"%s\",\"role\":%u,\"token\":\"%s\"}", CHAR__G__CurrentUser, (unsigned int)UINT8_T__G__CurrentRole, CHAR__G__SessionToken);
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", char__buf);
    }
    else if (int32_t__login == -1)
    {
        SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
        SIMPLE_WEB_SERVER_T__G__Server.send(409, "application/json",
            "{\"ok\":0,\"e\":\"busy\",\"msg\":\"یک ورود فعال هست؛ فقط دسترسی بالاتر می‌تواند جای آن را بگیرد\"}");
    }
    else if (int32_t__login == -2)
    {
        SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
        SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Retry-After", "30");
        SIMPLE_WEB_SERVER_T__G__Server.send(429, "application/json",
            "{\"ok\":0,\"e\":\"wait\",\"msg\":\"چند بار پشت‌سرهم اشتباه بود؛ کمی صبر کنید\"}");
    }
    else
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(401, "application/json", "{\"ok\":0,\"e\":\"wrong\"}");
    }
}
static void func__Simple_HttpLogout(void)
{
    if (func__Simple_SessionValid())
    {
        func__Simple_AuthLogout();
    }
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Set-Cookie",
        SIMPLE_SESSION_COOKIE "=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}
/* ==================== - /wifi ==================== */
static void func__Simple_HttpGetWifi(void)
{
    String string__reply = "{\"ssid\":";
    string__reply += func__Simple_JsonQuote(CHAR__G__WifiSsid);
    string__reply += ",\"passSet\":";
    string__reply += (CHAR__G__WifiPass[0] != '\0') ? "1}" : "0}";
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", string__reply);
}
/* ==================== - - - JSON ==================== */
static void func__Simple_HttpSetWifi(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__ssid, string__pass;
    if (!func__Simple_GetArg("ssid", string__ssid) || !func__Simple_GetArg("pass", string__pass))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"ssid/pass\"}");
        return;
    }
    if (string__ssid.length() < 1u || string__ssid.length() > SIMPLE_WIFI_SSID_MAX) { SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"ssid len\"}"); return; }
    if (string__pass.length() < 8u || string__pass.length() > SIMPLE_WIFI_PASS_MAX) { SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"pass len 8-32\"}"); return; }
    if (!BOOL__G__FsOk)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0,\"e\":\"storage\"}");
        return;
    }
    if ((strcmp(CHAR__G__WifiSsid, string__ssid.c_str()) == 0) &&
        (strcmp(CHAR__G__WifiPass, string__pass.c_str()) == 0))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1,\"reboot\":0}");
        return;
    }
    char char__oldSsid[sizeof(CHAR__G__WifiSsid)];
    char char__oldPass[sizeof(CHAR__G__WifiPass)];
    (void)memcpy(char__oldSsid, CHAR__G__WifiSsid, sizeof(char__oldSsid));
    (void)memcpy(char__oldPass, CHAR__G__WifiPass, sizeof(char__oldPass));
    (void)strncpy(CHAR__G__WifiSsid, string__ssid.c_str(), SIMPLE_WIFI_SSID_MAX);
    CHAR__G__WifiSsid[SIMPLE_WIFI_SSID_MAX] = '\0';
    (void)strncpy(CHAR__G__WifiPass, string__pass.c_str(), SIMPLE_WIFI_PASS_MAX);
    CHAR__G__WifiPass[SIMPLE_WIFI_PASS_MAX] = '\0';
    if (!func__Simple_WifiSave())
    {
        (void)memcpy(CHAR__G__WifiSsid, char__oldSsid, sizeof(char__oldSsid));
        (void)memcpy(CHAR__G__WifiPass, char__oldPass, sizeof(char__oldPass));
        SIMPLE_WEB_SERVER_T__G__Server.send(500, "application/json", "{\"ok\":0,\"e\":\"storage\"}");
        return;
    }
    BOOL__G__WifiRebootPending = true;
    UINT32_T__G__WifiRebootMs = (uint32_t)millis();
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1,\"reboot\":1}");
}
static void func__Simple_HttpClearHistory(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__type;
    if (!func__Simple_GetArg("type", string__type))
    {
        string__type = "all";
    }
    bool bool__ok = false;
    if (string__type == "charge") bool__ok = func__Simple_ChargeHistoryClear();
    else if (string__type == "charts") bool__ok = func__Simple_ChartsClear();
    else if (string__type == "diag") bool__ok = func__Simple_DiagClear();
    else if (string__type == "all")
    {
        bool bool__charge = func__Simple_ChargeHistoryClear();
        bool bool__charts = func__Simple_ChartsClear();
        bool bool__diag = func__Simple_DiagClear();
        bool__ok = bool__charge && bool__charts && bool__diag;
    }
    else
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"type\"}");
        return;
    }
    SIMPLE_WEB_SERVER_T__G__Server.send(bool__ok ? 200 : 500, "application/json",
        bool__ok ? "{\"ok\":1}" : "{\"ok\":0,\"e\":\"storage\"}");
}
/**
* @return JSON ok → e=1 → e=2
 */
static void func__Simple_HttpLatchReset(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__n;
    /* Accept only the three canonical commands, before touching the slot queue. */
    if (!func__Simple_GetArg("n", string__n) ||
        ((string__n != "5") && (string__n != "6") && (string__n != "8")))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json",
            "{\"ok\":0,\"e\":\"reset\",\"allowed\":[5,6,8]}");
        return;
    }
    uint8_t uint8_t__n = (uint8_t)(string__n[0] - '0');
    bool bool__ok = false;
    if (uint8_t__n == 5u)
    {
        bool__ok = func__Simple_QueueSlot(SIMPLE_SLOT_EVENTS_ID, 0u);
        bool__ok = (func__Simple_QueueSlot(SIMPLE_SLOT_CYCLES_ID, 0u) == true) && bool__ok;
        bool__ok = (func__Simple_QueueSlot(SIMPLE_SLOT_LATCH_ID, 0u) == true) && bool__ok;
    }
    else if (uint8_t__n == 6u)
    {
        bool__ok = func__Simple_QueueSlot(SIMPLE_SLOT_DEAD_MASK_ID, 0u);
    }
    else
    {
        bool__ok = func__Simple_QueueSlot(SIMPLE_SLOT_WEAK_RESET_ID, 0u);
    }
    char char__A__Out[48];
    (void)snprintf(char__A__Out, sizeof(char__A__Out), "{\"ok\":%u,\"n\":%u}",
                   bool__ok ? 1u : 0u, (unsigned)uint8_t__n);
    SIMPLE_WEB_SERVER_T__G__Server.send(bool__ok ? 200 : 409, "application/json", char__A__Out);
}
/**
* @return JSON id/val/known/rep/req/nak/busy/q
 */
static void func__Simple_HttpLatchState(void)
{
    uint8_t uint8_t__i;
    uint8_t uint8_t__pending = 0u;
    char char__A__Out[160];
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_SLOT_QUEUE_LEN; uint8_t__i++)
    {
        if (BOOL__G__SlotQueuePending[uint8_t__i])
        {
            uint8_t__pending++;
        }
    }
    (void)snprintf(char__A__Out, sizeof(char__A__Out),
                   "{\"ok\":1,\"id\":%u,\"val\":%lu,\"known\":%u,\"rep\":%lu,"
                   "\"req\":%lu,\"nak\":%lu,\"busy\":%u,\"q\":%u}",
                   (unsigned)UINT8_T__G__SlotLastId,
                   (unsigned long)UINT32_T__G__SlotLastValue,
                   BOOL__G__SlotLastKnown ? 1u : 0u,
                   (unsigned long)UINT32_T__G__SlotEchoCount,
                   (unsigned long)UINT32_T__G__SlotRequestCount,
                   (unsigned long)UINT32_T__G__SlotNakCount,
                   (unsigned)UINT8_T__G__SlotBusyReject,
                   (unsigned)uint8_t__pending);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", char__A__Out);
}
static char CHAR__G__LogTailBuffer[3600];
static void func__Simple_ParseDayArg(bool *bool__ptr_hasDay, uint32_t *uint32_t__ptr_day)
{
    String string__day;
    *bool__ptr_hasDay = false;
    *uint32_t__ptr_day = SIMPLE_LOG_DAY_AUTO;
    if (func__Simple_GetArg("day", string__day))
    {
        string__day.trim();
        /* [EN] only eight digits: anything else would just make a file name
           that cannot exist, so the request is answered as "that day" = none. */
        if ((string__day.length() > 0u) && (string__day.length() <= 8u))
        {
            bool bool__digits = true;
            size_t size_t__i;
            for (size_t__i = 0u; size_t__i < string__day.length(); size_t__i++)
            {
                if ((string__day[size_t__i] < '0') || (string__day[size_t__i] > '9'))
                {
                    bool__digits = false;
                    break;
                }
            }
            if (bool__digits != false)
            {
                *bool__ptr_hasDay = true;
                *uint32_t__ptr_day = (uint32_t)strtoul(string__day.c_str(), NULL, 10);
            }
        }
    }
}
/**
 * @brief Accept automatic browser time; empty requests remain admin session probes.
 */
static void func__Simple_HttpClockSet(void)
{
    String string__sec, string__tz;
    int32_t int32_t__sec = 0, int32_t__tz = INT32_T__G__LogTzSec;
    bool bool__hasSec = func__Simple_GetArg("sec", string__sec);
    if (!bool__hasSec && !func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    if ((bool__hasSec && (!func__Simple_ParseInt(string__sec.c_str(), &int32_t__sec) ||
         int32_t__sec < (int32_t)SIMPLE_TIME_VALID_MIN)) ||
        (func__Simple_GetArg("tz", string__tz) &&
         (!func__Simple_ParseInt(string__tz.c_str(), &int32_t__tz) ||
          int32_t__tz < -50400 || int32_t__tz > 50400)))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"time\"}");
        return;
    }
    INT32_T__G__LogTzSec = int32_t__tz;
    if (bool__hasSec)
    {
        func__Simple_TimeSet((uint32_t)int32_t__sec, true);
        BOOL__G__BrowserTimePending = true;
        BOOL__G__TimeFromDisk = false;
        UINT32_T__G__TimePushMs = (uint32_t)millis();
        UINT32_T__G__TimePushArmedMs = UINT32_T__G__TimePushMs;
        func__Simple_TimeSave();
        func__Simple_SendTimeSync(func__Simple_NowSec());
    }
    (void)snprintf(CHAR__G__JsonBuffer, SIMPLE_JSON_BUFFER_SIZE,
                   "{\"ok\":%u,\"sec\":%lu}",
                   bool__hasSec ? 1u : 0u,
                   (unsigned long)func__Simple_NowSec());
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}
/* ==================== Format verification ==================== */
/**
 * @brief Verify saved encrypted settings without reloading RAM or exposing secrets.
 * @param char__ptr_path Settings file path.
 * @return bool True when the file can be read and its authentication tag is valid.
 */
static bool func__Simple_FormatSettingsValid(const char *char__ptr_path)
{
    File file__read = LittleFS.open(char__ptr_path, "r");
    if (!file__read) return false;
    String string__raw = file__read.readString();
    file__read.close();
    String string__plain;
    return (func__Simple_CryptoDecrypt(string__raw, string__plain) == 2u);
}
/* ==================== Explicit admin format ==================== */
/**
 * @brief Format, remount and verify storage without changing the active session.
 */
static void func__Simple_HttpEventFormat(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "text/plain", "admin");
        return;
    }
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    LittleFS.end();
    BOOL__G__FsOk = false;
    BOOL__G__FsMountFailed = true;
    if (!LittleFS.format())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(500, "text/plain", "fail");
        return;
    }
    func__SimpleLog_ResetStateAfterFormat();
    /* Clear RAM after successful format even when remount subsequently fails. */
    (void)func__Simple_ChargeHistoryClear(false);
    (void)func__Simple_ChartsClear(false);
    (void)func__Simple_DiagClear(false);
#if defined(ESP8266)
    (void)LittleFS.setConfig(LittleFSConfig(false));
    BOOL__G__FsOk = LittleFS.begin();
#else
    BOOL__G__FsOk = LittleFS.begin(false);
#endif
    BOOL__G__FsMountFailed = (BOOL__G__FsOk == false);
    if (!BOOL__G__FsOk)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(500, "text/plain", "mount");
        return;
    }
    /* [EN] Probe the closed file, not just the return value of format(). */
    bool bool__writable = false;
    {
        File file__probe = LittleFS.open("/fs-check", "w");
        if (file__probe)
        {
            bool__writable = (file__probe.print("fs-ok") == 5u);
            file__probe.close();
        }
    }
    if (bool__writable)
    {
        File file__probe = LittleFS.open("/fs-check", "r");
        bool__writable = file__probe && (file__probe.readString() == "fs-ok");
        file__probe.close();
    }
    (void)LittleFS.remove("/fs-check");
    func__Simple_AuthSave();
    func__Simple_WifiSave();
    UINT32_T__G__LastSavedTimeSec = 0u;
    func__Simple_TimeSave();
    if (!bool__writable || !func__Simple_FormatSettingsValid(SIMPLE_USERS_FILE) ||
        !func__Simple_FormatSettingsValid(SIMPLE_WIFI_FILE))
    {
        UINT8_T__G__LogWriteFault = 1u;
        SIMPLE_WEB_SERVER_T__G__Server.send(500, "text/plain", "write");
        return;
    }
    func__SimpleLog_Begin();
    /* [EN] Long flash operations must not consume the owner's idle deadline. */
    UINT32_T__G__SessionStartMs = (uint32_t)millis();
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/plain", "ok");
}
/* ==================== Panel-only factory reset ==================== */
/** @brief Erase ESP data only after administrator authentication and explicit destructive confirmation. */
static void func__Simple_HttpFactoryReset(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__confirm;
    if (!func__Simple_GetArg("confirm", string__confirm) || string__confirm != "ERASE_ESP")
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"confirm\"}");
        return;
    }
    LittleFS.end();
    BOOL__G__FsOk = false;
    bool bool__erased = LittleFS.format();
#if defined(ESP8266)
    (void)LittleFS.setConfig(LittleFSConfig(false));
    BOOL__G__FsOk = LittleFS.begin();
#else
    BOOL__G__FsOk = LittleFS.begin(false);
#endif
    BOOL__G__FsMountFailed = !BOOL__G__FsOk;
    if (!bool__erased)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(500, "application/json", "{\"ok\":0,\"e\":\"format\"}");
        return;
    }
    /* No old RAM value may be saved after the erase, even if verification fails. */
    BOOL__G__FactoryResetPending = true;
    UINT32_T__G__FactoryResetMs = (uint32_t)millis();
    bool bool__verified = false;
    if (BOOL__G__FsOk)
    {
        File file__probe = LittleFS.open("/fs-check", "w");
        bool__verified = file__probe && (file__probe.print("fs-ok") == 5u);
        file__probe.close();
        if (bool__verified)
        {
            File file__read = LittleFS.open("/fs-check", "r");
            bool__verified = file__read && (file__read.readString() == "fs-ok");
            file__read.close();
        }
        bool__verified = LittleFS.remove("/fs-check") && bool__verified;
    }
    func__Simple_AuthLogout();
    func__Simple_LoginClear();
    BOOL__G__FsOk = false;
    BOOL__G__WifiRebootPending = false;
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(bool__verified ? 200 : 500, "application/json",
        bool__verified ? "{\"ok\":1,\"erased\":1,\"restart\":1}" : "{\"ok\":0,\"e\":\"verify\",\"erased\":1,\"restart\":1}");
}
/* ==================== Erased-panel reboot barrier ==================== */
/** @brief Stop UART processing and all persistence until the wiped ESP restarts; never command STM32. */
static bool func__Simple_FactoryResetTick(void)
{
    if (!BOOL__G__FactoryResetPending) return false;
    if ((uint32_t)((uint32_t)millis() - UINT32_T__G__FactoryResetMs) >= 1500u) ESP.restart();
    delay(1u);
    return true;
}
static void func__Simple_HttpEventLog(void)
{
    bool bool__hasDay = false;
    uint32_t uint32_t__day = SIMPLE_LOG_DAY_AUTO;
    /* 0 = no file system, 2 = the flash refused the write (both are served
       from the RAM ring), 1 = the rows come from the flash. */
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("X-Log-Fs",
        (BOOL__G__FsOk == false) ? (BOOL__G__FsMountFailed ? "3" : "0")
                                : ((UINT8_T__G__LogWriteFault != 0u) ? "2" : "1"));
    (void)snprintf(CHAR__G__LogLostText, sizeof(CHAR__G__LogLostText), "%u",
                   (unsigned int)UINT16_T__G__LogDroppedRows);
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("X-Log-Lost", CHAR__G__LogLostText);
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    func__Simple_ParseDayArg(&bool__hasDay, &uint32_t__day);
    if ((bool__hasDay != false) && (uint32_t__day == 0u))
    {
        uint32_t__day = SIMPLE_LOG_DAY_ZERO;
    }
    (void)func__SimpleLog_Tail(CHAR__G__LogTailBuffer, sizeof(CHAR__G__LogTailBuffer),
                               uint32_t__day, SIMPLE_LOG_TAIL_LINES);
    if (BOOL__G__LogClockGap && !BOOL__G__LogClockGapEmitted &&
        (!bool__hasDay || uint32_t__day == SIMPLE_LOG_DAY_ZERO))
    {
        size_t size_t__used = strlen(CHAR__G__LogTailBuffer);
        if (size_t__used + strlen(SIMPLE_LOG_CLOCK_GAP_ROW) < sizeof(CHAR__G__LogTailBuffer))
            (void)strcpy(CHAR__G__LogTailBuffer + size_t__used, SIMPLE_LOG_CLOCK_GAP_ROW);
    }
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/plain", CHAR__G__LogTailBuffer);
}
static void func__Simple_HttpEventDays(void)
{
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    (void)func__SimpleLog_Days(CHAR__G__LogTailBuffer, sizeof(CHAR__G__LogTailBuffer));
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/plain", CHAR__G__LogTailBuffer);
}
/*
 * [EN] One request for the whole history: every day file decoded once, oldest
 * first, behind a single CSV legend. 90 days used to mean 90 downloads, and the
 * browser blocks those after the third one anyway.
 */
static void func__Simple_HttpDownloadAllDays(void)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    char char__row[SIMPLE_LOG_RAM_ROW_MAX];
    uint8_t uint8_t__slot;
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Content-Disposition",
                                              "attachment; filename=evt-all.csv");
    if (BOOL__G__FsOk == false)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(404, "text/plain", "empty");
        return;
    }
    if (BOOL__G__LogScanValid == false)
    {
        (void)func__SimpleLog_ScanDays(true);
    }
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/csv", "");
    SIMPLE_WEB_SERVER_T__G__Server.sendContent(SIMPLE_LOG_HEADER);
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        SimpleLogCodec::State codec;
        int int__kind;
        func__SimpleLog_PathOf(char__ptr_path, UINT32_T__G__LogIndexDay[uint8_t__slot]);
        File file__day = LittleFS.open(char__ptr_path, "r");
        if (!file__day)
        {
            continue;
        }
        int__kind = SimpleLogCodec::begin(file__day, codec);
        if (int__kind == 1)
        {
            while (SimpleLogCodec::next(file__day, codec))
            {
                if (SimpleLogCodec::text(codec, char__row, sizeof(char__row)) != 0u)
                {
                    SIMPLE_WEB_SERVER_T__G__Server.sendContent(char__row);
                }
            }
        }
        file__day.close();
    }
    if (BOOL__G__LogClockGap && !BOOL__G__LogClockGapEmitted)
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(SIMPLE_LOG_CLOCK_GAP_ROW);
    SIMPLE_WEB_SERVER_T__G__Server.sendContent("");
}
static void func__Simple_HttpEventDownload(void)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    char char__ptr_name[64];
    bool bool__hasDay = false;
    uint32_t uint32_t__day = SIMPLE_LOG_DAY_AUTO;
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    if (SIMPLE_WEB_SERVER_T__G__Server.hasArg("all") &&
        (SIMPLE_WEB_SERVER_T__G__Server.arg("all") != "0"))
    {
        func__Simple_HttpDownloadAllDays();
        return;
    }
    func__Simple_ParseDayArg(&bool__hasDay, &uint32_t__day);
    if (BOOL__G__FsOk == false)
    {
        size_t size_t__hdrLen;
        if (UINT8_T__G__LogRamCount == 0u)
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(404, "text/plain", "empty");
            return;
        }
        size_t__hdrLen = strlen(SIMPLE_LOG_HEADER);
        if (size_t__hdrLen >= sizeof(CHAR__G__LogTailBuffer))
        {
            size_t__hdrLen = 0u;
        }
        else
        {
            (void)memcpy(CHAR__G__LogTailBuffer, SIMPLE_LOG_HEADER, size_t__hdrLen);
        }
        (void)func__SimpleLog_Tail(&CHAR__G__LogTailBuffer[size_t__hdrLen],
                                   sizeof(CHAR__G__LogTailBuffer) - size_t__hdrLen,
                                   SIMPLE_LOG_DAY_AUTO, SIMPLE_LOG_RAM_ROWS);
        SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Content-Disposition",
                                                  "attachment; filename=evt-ram.csv");
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/csv", CHAR__G__LogTailBuffer);
        return;
    }
    if (bool__hasDay == false)
    {
        uint32_t__day = func__SimpleLog_TodayCode();
        func__SimpleLog_PathOf(char__ptr_path, uint32_t__day);
        if (LittleFS.exists(char__ptr_path) == false)
        {
            /* [EN] today has no file yet: fall back to the newest day that
               exists, after a real scan (the cache may be older than the boot). */
            if ((UINT8_T__G__LogIndexCount == 0u) || (BOOL__G__LogScanValid == false))
            {
                (void)func__SimpleLog_ScanDays(true);
            }
            if (UINT8_T__G__LogIndexCount == 0u)
            {
                SIMPLE_WEB_SERVER_T__G__Server.send(404, "text/plain", "empty");
                return;
            }
            uint32_t__day = UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount - 1u];
        }
    }
    func__SimpleLog_PathOf(char__ptr_path, uint32_t__day);
    if (LittleFS.exists(char__ptr_path) == false)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(404, "text/plain", "missing");
        return;
    }
    (void)snprintf(char__ptr_name, sizeof(char__ptr_name),
                   "attachment; filename=evt-%08lu.csv", (unsigned long)uint32_t__day);
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Content-Disposition", char__ptr_name);
    File file__day = LittleFS.open(char__ptr_path, "r");
    if (!file__day)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(503, "text/plain", "fs");
        return;
    }
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/csv", "");
    {
        /*
         * [EN] The day file holds CRC-protected records, so the download is
         * rebuilt as CSV here, one row at a time, with the column legend in
         * front. A legacy text file is still streamed exactly as it sits.
         */
        SimpleLogCodec::State codec;
        const int int__kind = SimpleLogCodec::begin(file__day, codec);
        if (int__kind == 1)
        {
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(SIMPLE_LOG_HEADER);
            while (SimpleLogCodec::next(file__day, codec))
            {
                char char__row[SIMPLE_LOG_RAM_ROW_MAX];
                if (SimpleLogCodec::text(codec, char__row, sizeof(char__row)) == 0u)
                {
                    continue;
                }
                SIMPLE_WEB_SERVER_T__G__Server.sendContent(char__row);
            }
        }
        else
        {
            if (int__kind == 0)
            {
                (void)file__day.seek(0u);
                while (file__day.available())
                {
                    String string__line = file__day.readStringUntil('\n');
                    if (string__line.length() > 0u)
                    {
                        SIMPLE_WEB_SERVER_T__G__Server.sendContent(string__line);
                    }
                    SIMPLE_WEB_SERVER_T__G__Server.sendContent("\n");
                }
            }
        }
    }
    file__day.close();
    SIMPLE_WEB_SERVER_T__G__Server.sendContent("");
}
static void func__Simple_HttpEventClear(void)
{
    uint8_t uint8_t__removed;
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    if (!BOOL__G__FsOk)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0,\"e\":\"storage\"}");
        return;
    }
    uint8_t__removed = func__SimpleLog_Clear();
    bool bool__ok = func__SimpleLog_ScanDays(true) == 0u && !LittleFS.exists(SIMPLE_TIME_GAP_FILE);
    SIMPLE_WEB_SERVER_T__G__Server.send(bool__ok ? 200 : 500, "application/json",
        String(bool__ok ? "{\"ok\":1,\"removed\":" : "{\"ok\":0,\"e\":\"storage\",\"removed\":") +
        String((unsigned int)uint8_t__removed) + "}");
}
static void func__Simple_HttpEventSnap(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    bool bool__stored;
    bool__stored = func__SimpleLog_SnapshotNow();
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    (void)snprintf(CHAR__G__JsonBuffer, SIMPLE_JSON_BUFFER_SIZE,
                   "{\"ok\":%u}", (bool__stored != false) ? 1u : 0u);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}
/* ==================== Single file deletion ==================== */
/**
 * @brief Delete only an exact root file after admin authorization; keep RAM settings/session.
 */
static void func__Simple_HttpFileDelete(void)
{
    String string__name;
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    if (!func__Simple_GetArg("name", string__name) || string__name.length() < 2u ||
        string__name.length() > 255u || string__name[0] != '/' ||
        string__name.indexOf('/', 1u) >= 0 || string__name.indexOf('\\') >= 0 ||
        strcmp(string__name.c_str(), "/.") == 0 || strcmp(string__name.c_str(), "/..") == 0 ||
        strlen(string__name.c_str()) != string__name.length())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"path\"}");
        return;
    }
    for (size_t size_t__i = 0u; size_t__i < string__name.length(); size_t__i++)
    {
        if ((unsigned char)string__name[size_t__i] < 0x20u)
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"path\"}");
            return;
        }
    }
    if (!BOOL__G__FsOk)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0,\"e\":\"fs\"}");
        return;
    }
    if (!LittleFS.exists(string__name.c_str()))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(404, "application/json", "{\"ok\":0,\"e\":\"missing\"}");
        return;
    }
#if defined(ESP32)
    {
        File file__target = LittleFS.open(string__name.c_str(), "r");
        bool bool__directory = file__target && file__target.isDirectory();
        file__target.close();
        if (bool__directory)
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"path\"}");
            return;
        }
    }
#endif
    if (!LittleFS.remove(string__name.c_str()) || LittleFS.exists(string__name.c_str()))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(500, "application/json", "{\"ok\":0,\"e\":\"remove\"}");
        return;
    }
    uint32_t uint32_t__day = 0u;
    BOOL__G__LogScanValid = false;
    if (func__SimpleLog_DayFromName(string__name.c_str(), &uint32_t__day) &&
        uint32_t__day == UINT32_T__G__LogDayCode)
    {
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogDayFileReady = false;
        BOOL__G__LogDrainWait = false;
    }
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}
static void func__Simple_HttpEventFsStatus(void)
{
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    String string__offset;
    int32_t int32_t__offset = 0;
    if (func__Simple_GetArg("offset", string__offset) &&
        (!func__Simple_ParseInt(string__offset.c_str(), &int32_t__offset) ||
         int32_t__offset < 0 || int32_t__offset > 65535))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"offset\"}");
        return;
    }
    bool bool__admin = func__Simple_SessionValid() && UINT8_T__G__CurrentRole == 1u;
    size_t size_t__written = func__SimpleLog_FsStatusJson(CHAR__G__LogTailBuffer,
        sizeof(CHAR__G__LogTailBuffer), bool__admin, (uint16_t)int32_t__offset);
    SIMPLE_WEB_SERVER_T__G__Server.send(size_t__written ? 200 : 500, "application/json",
        size_t__written ? CHAR__G__LogTailBuffer : "{\"ok\":0}");
}
