/* simple_http.h - هندلرهای HTTP پنل سه صفحه‌ای
   فقط فارسی - نمودار، کاربر، تنظیمات، دیاگ، ساعت، احراز هویت
   اصلاح شده: پشتیبانی JSON body + boardTime در تله‌متری */

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

/* ===== کمک JSON ساده - استخراج از plain body ===== */
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
        /* عدد یا بولین بدون کوتیشن */
        int int__end = int__pos;
        while (int__end < (int)string__json.length() && string__json[int__end] != ',' && string__json[int__end] != '}' && string__json[int__end] != ' ' && string__json[int__end] != '\r' && string__json[int__end] != '\n') int__end++;
        string__out = string__json.substring(int__pos, int__end);
        string__out.trim();
        return string__out.length() > 0;
    }
}

/* گرفتن آرگومان با پشتیبانی JSON plain */
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

/* ==================== صفحه اصلی ==================== */
static void func__Simple_HttpRoot(void)
{
    BOOL__G__BrowserSeen = true;
    UINT32_T__G__LastBrowserPollMs = (uint32_t)millis();
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/html", "");
    static const size_t SIZE_T__CHUNK = 2048u;
    const size_t SIZE_T__BYTES = sizeof(SIMPLE_PANEL_HTML) - 1u;
    for (size_t SIZE_T__OFFSET = 0u; SIZE_T__OFFSET < SIZE_T__BYTES; SIZE_T__OFFSET += SIZE_T__CHUNK)
    {
        const size_t SIZE_T__REMAINING = SIZE_T__BYTES - SIZE_T__OFFSET;
        const size_t SIZE_T__COUNT = (SIZE_T__REMAINING < SIZE_T__CHUNK) ? SIZE_T__REMAINING : SIZE_T__CHUNK;
        SIMPLE_WEB_SERVER_T__G__Server.sendContent_P(SIMPLE_PANEL_HTML + SIZE_T__OFFSET, SIZE_T__COUNT);
        func__Simple_DrainSerial();
    }
}

/* ==================== تله‌متری زنده - با boardTime ==================== */
static void func__Simple_HttpTelemetry(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__ageMs = uint32_t__nowMs - UINT32_T__G__LastTlmMs;
    uint32_t uint32_t__panelTimeSec = func__Simple_NowSec();
    bool bool__online = BOOL__G__TlmSeen && (uint32_t__ageMs <= SIMPLE_LINK_TIMEOUT_MS);
    uint8_t uint8_t__index;
    size_t size_t__used;

    BOOL__G__BrowserSeen = true;
    UINT32_T__G__LastBrowserPollMs = uint32_t__nowMs;
    func__Simple_ChartTick();

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, SIMPLE_JSON_BUFFER_SIZE,
        "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"fl2\":%u,\"n\":%lu,\"ce\":%lu,\"vm\":%lu,\"inputPresent\":%u,\"user\":\"%s\",\"role\":%u,\"theme\":%u,\"boardTime\":%lu,\"bt\":%lu,\"timeOk\":%u,\"pvOk\":%u,\"recovery\":%u,"
        /* دست‌دادن دوطرفه (v1.111): lko = پاسخ تازهٔ برد داریم، bfw = نسخهٔ فرم‌ور
           برد، brx = فریم سالم از دید برد، bce/bvm = خطای CRC و ناهم‌نسخگی از دید
           برد، bpc = تعداد پارامتر برد، bpr = دورهٔ تله‌متری، lac = سن آخرین پاسخ. */
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
        CHAR__G__CurrentUser,
        (unsigned int)UINT8_T__G__CurrentRole,
        (unsigned int)UINT8_T__G__Theme,
        (unsigned long)uint32_t__panelTimeSec,
        (unsigned long)uint32_t__panelTimeSec,
        BOOL__G__TimeValid ? 1u : 0u,
        BOOL__G__BoardParamSeen ? 1u : 0u,
        BOOL__G__RecoveryHappened ? 1u : 0u,
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

    for (uint8_t__index = 0u; uint8_t__index < SIMPLE_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used],
            SIMPLE_JSON_BUFFER_SIZE - size_t__used,
            "%s%lu", char__ptr_sep, (unsigned long)UINT32_T__G__TlmField[uint8_t__index]);
    }
    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used],
        SIMPLE_JSON_BUFFER_SIZE - size_t__used, "],\"pv\":[");

    /* پارامترهای خود برد به همان ترتیبی که پنل انتظار دارد */
    for (uint8_t__index = 0u; uint8_t__index < SIMPLE_PARAM_UI_COUNT; uint8_t__index++)
    {
        const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used],
            SIMPLE_JSON_BUFFER_SIZE - size_t__used,
            "%s%lu", char__ptr_sep,
            (unsigned long)func__Simple_BoardParam(UINT8_T__G__ParamUiIds[uint8_t__index]));
    }
    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used],
        SIMPLE_JSON_BUFFER_SIZE - size_t__used, "]}");

    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/* ==================== داده نمودارها ==================== */
static void func__Simple_HttpCharts(void)
{
    String string__range = SIMPLE_WEB_SERVER_T__G__Server.hasArg("range") ? SIMPLE_WEB_SERVER_T__G__Server.arg("range") : "1d";
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "");
    uint8_t uint8_t__i, uint8_t__idx;

    if (string__range == "7d")
    {
        String s; s.reserve(512);
        s = "{\"range\":\"7d\",\"count\":"; s += String(UINT8_T__G__Chart7D_Count); s += ",\"pct\":[";
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(s);
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart7D_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart7D_Index + SIMPLE_CHART_7D_SIZE - UINT8_T__G__Chart7D_Count + uint8_t__i) % SIMPLE_CHART_7D_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart7D_Pct[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vin\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart7D_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart7D_Index + SIMPLE_CHART_7D_SIZE - UINT8_T__G__Chart7D_Count + uint8_t__i) % SIMPLE_CHART_7D_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart7D_Vin[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vpack\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart7D_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart7D_Index + SIMPLE_CHART_7D_SIZE - UINT8_T__G__Chart7D_Count + uint8_t__i) % SIMPLE_CHART_7D_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart7D_Vpack[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"time\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart7D_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart7D_Index + SIMPLE_CHART_7D_SIZE - UINT8_T__G__Chart7D_Count + uint8_t__i) % SIMPLE_CHART_7D_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart7D_Time[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("]}");
    }
    else if (string__range == "1m")
    {
        String s; s.reserve(512);
        s = "{\"range\":\"1m\",\"count\":"; s += String(UINT8_T__G__Chart1M_Count); s += ",\"pct\":[";
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(s);
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1M_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1M_Index + SIMPLE_CHART_1M_SIZE - UINT8_T__G__Chart1M_Count + uint8_t__i) % SIMPLE_CHART_1M_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart1M_Pct[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vin\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1M_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1M_Index + SIMPLE_CHART_1M_SIZE - UINT8_T__G__Chart1M_Count + uint8_t__i) % SIMPLE_CHART_1M_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart1M_Vin[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vpack\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1M_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1M_Index + SIMPLE_CHART_1M_SIZE - UINT8_T__G__Chart1M_Count + uint8_t__i) % SIMPLE_CHART_1M_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart1M_Vpack[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"time\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1M_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1M_Index + SIMPLE_CHART_1M_SIZE - UINT8_T__G__Chart1M_Count + uint8_t__i) % SIMPLE_CHART_1M_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart1M_Time[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("]}");
    }
    else if (string__range == "3m")
    {
        String s; s.reserve(512);
        s = "{\"range\":\"3m\",\"count\":"; s += String(UINT8_T__G__Chart3M_Count); s += ",\"pct\":[";
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(s);
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart3M_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart3M_Index + SIMPLE_CHART_3M_SIZE - UINT8_T__G__Chart3M_Count + uint8_t__i) % SIMPLE_CHART_3M_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart3M_Pct[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vin\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart3M_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart3M_Index + SIMPLE_CHART_3M_SIZE - UINT8_T__G__Chart3M_Count + uint8_t__i) % SIMPLE_CHART_3M_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart3M_Vin[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vpack\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart3M_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart3M_Index + SIMPLE_CHART_3M_SIZE - UINT8_T__G__Chart3M_Count + uint8_t__i) % SIMPLE_CHART_3M_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart3M_Vpack[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"time\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart3M_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart3M_Index + SIMPLE_CHART_3M_SIZE - UINT8_T__G__Chart3M_Count + uint8_t__i) % SIMPLE_CHART_3M_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart3M_Time[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("]}");
    }
    else
    {
        String s; s.reserve(512);
        s = "{\"range\":\"1d\",\"count\":"; s += String(UINT8_T__G__Chart1D_Count); s += ",\"pct\":[";
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(s);
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1D_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - UINT8_T__G__Chart1D_Count + uint8_t__i) % SIMPLE_CHART_1D_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart1D_Pct[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vin\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1D_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - UINT8_T__G__Chart1D_Count + uint8_t__i) % SIMPLE_CHART_1D_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart1D_Vin[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vpack\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1D_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - UINT8_T__G__Chart1D_Count + uint8_t__i) % SIMPLE_CHART_1D_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart1D_Vpack[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"time\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1D_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - UINT8_T__G__Chart1D_Count + uint8_t__i) % SIMPLE_CHART_1D_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart1D_Time[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("]}");
    }
}

/* ==================== دیاگ ==================== */
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
    /* «boardTime» همان ساعت خود STM32 است (گزارش تلمتری) و «panelTime» ساعت
       پنل؛ هر دو نمایش داده می‌شوند تا بشود تفاوت را دید */
    s += ",\"boardTime\":"; s += String(UINT32_T__G__BoardTimeSec);
    s += ",\"panelTime\":"; s += String(func__Simple_NowSec());
    s += ",\"timeOk\":"; s += String(BOOL__G__TimeValid ? 1u : 0u);
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
        ch += "}";
        SIMPLE_WEB_SERVER_T__G__Server.sendContent(ch);
    }
    SIMPLE_WEB_SERVER_T__G__Server.sendContent("]}");
}

/* ==================== همگام‌سازی ساعت - JSON + query ==================== */
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

    /* اختلاف منطقهٔ زمانی هم از همان گوشی می‌آید تا نام فایل روزانهٔ تاریخچه
       سرِ نیمه‌شب خودِ اپراتور عوض شود، نه نیمه‌شب UTC. */
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

    /* ساعت را در پنل تنظیم، روی فایل ذخیره و به برد هم بگو؛ از این لحظه
       لنگر ساعت در پنل می‌ماند و بین فریم‌ها هم راه می‌رود */
    func__Simple_TimeSet((uint32_t)int32_t__ts, true);
    func__Simple_TimeSave();
    func__Simple_SendTimeSync(func__Simple_NowSec());
    UINT32_T__G__BoardTimeSec = func__Simple_NowSec();
    BOOL__G__TimePushed = true;

    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/* ==================== لاگین - JSON + form ==================== */
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

    if (func__Simple_AuthLogin(string__user.c_str(), string__pass.c_str()))
    {
        char char__buf[128];
        (void)snprintf(char__buf, sizeof(char__buf), "{\"ok\":1,\"user\":\"%s\",\"role\":%u}", CHAR__G__CurrentUser, (unsigned int)UINT8_T__G__CurrentRole);
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", char__buf);
    }
    else
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(401, "application/json", "{\"ok\":0,\"e\":\"wrong\"}");
    }
}

/* ==================== خروج ==================== */
static void func__Simple_HttpLogout(void)
{
    func__Simple_AuthLogout();
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/* ==================== لیست کاربران ==================== */
static void func__Simple_HttpUsers(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }

    size_t size_t__used = 0u;
    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, SIMPLE_JSON_BUFFER_SIZE, "{\"users\":[");
    bool bool__first = true;
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        if (!SIMPLE_USER_T__G__Users[uint8_t__i].active) continue;
        if (!bool__first) size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], SIMPLE_JSON_BUFFER_SIZE - size_t__used, ",");
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], SIMPLE_JSON_BUFFER_SIZE - size_t__used,
            "{\"name\":\"%s\",\"role\":%u}", SIMPLE_USER_T__G__Users[uint8_t__i].name, (unsigned int)SIMPLE_USER_T__G__Users[uint8_t__i].role);
        bool__first = false;
    }
    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], SIMPLE_JSON_BUFFER_SIZE - size_t__used, "]}");
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/* ==================== افزودن کاربر - JSON ==================== */
static void func__Simple_HttpAddUser(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__user, string__pass, string__roleStr;
    if (!func__Simple_GetArg("user", string__user) || !func__Simple_GetArg("pass", string__pass))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }
    int32_t int32_t__role = 0;
    if (func__Simple_GetArg("role", string__roleStr))
    {
        func__Simple_ParseInt(string__roleStr.c_str(), &int32_t__role);
    }
    if (func__Simple_AddUser(string__user.c_str(), string__pass.c_str(), (uint8_t)int32_t__role))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
    }
    else
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"full\"}");
    }
}

/* ==================== حذف کاربر - JSON ==================== */
static void func__Simple_HttpDelUser(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__user;
    if (!func__Simple_GetArg("user", string__user))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }
    if (func__Simple_DeleteUser(string__user.c_str()))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
    }
    else
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
    }
}

/* ==================== هندلر جدید: POST /users با action ==================== */
static void func__Simple_HttpUsersPost(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__action, string__user, string__pass, string__roleStr;
    func__Simple_GetArg("action", string__action);

    if (string__action == "del")
    {
        if (!func__Simple_GetArg("user", string__user))
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"user\"}");
            return;
        }
        if (func__Simple_DeleteUser(string__user.c_str()))
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
        }
        else
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        }
        return;
    }
    else /* add */
    {
        if (!func__Simple_GetArg("user", string__user) || !func__Simple_GetArg("pass", string__pass))
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"user/pass\"}");
            return;
        }
        int32_t int32_t__role = 0;
        if (func__Simple_GetArg("role", string__roleStr))
        {
            func__Simple_ParseInt(string__roleStr.c_str(), &int32_t__role);
        }
        if (func__Simple_AddUser(string__user.c_str(), string__pass.c_str(), (uint8_t)int32_t__role))
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
        }
        else
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"full\"}");
        }
    }
}

/* ==================== گرفتن تم ==================== */
static void func__Simple_HttpGetTheme(void)
{
    char char__buf[64];
    (void)snprintf(char__buf, sizeof(char__buf), "{\"theme\":%u}", (unsigned int)UINT8_T__G__Theme);
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", char__buf);
}

/* ==================== تنظیم تم - فقط ادمین ==================== */
static void func__Simple_HttpSetTheme(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__themeStr;
    int32_t int32_t__theme = 0;
    if (!func__Simple_GetArg("theme", string__themeStr))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }
    func__Simple_ParseInt(string__themeStr.c_str(), &int32_t__theme);
    if (func__Simple_SetTheme((uint8_t)int32_t__theme))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
    }
    else
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
    }
}

/* ==================== وای‌فای - گرفتن تنظیمات ==================== */
static void func__Simple_HttpGetWifi(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    char char__buf[128];
    (void)snprintf(char__buf, sizeof(char__buf), "{\"ssid\":\"%s\",\"pass\":\"%s\"}", CHAR__G__WifiSsid, CHAR__G__WifiPass);
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", char__buf);
}

/* ==================== وای‌فای - تنظیم - فقط ادمین - JSON ==================== */
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

    if ((strcmp(CHAR__G__WifiSsid, string__ssid.c_str()) == 0) &&
        (strcmp(CHAR__G__WifiPass, string__pass.c_str()) == 0))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1,\"reboot\":0}");
        return;
    }

    (void)strncpy(CHAR__G__WifiSsid, string__ssid.c_str(), SIMPLE_WIFI_SSID_MAX);
    CHAR__G__WifiSsid[SIMPLE_WIFI_SSID_MAX] = '\0';
    (void)strncpy(CHAR__G__WifiPass, string__pass.c_str(), SIMPLE_WIFI_PASS_MAX);
    CHAR__G__WifiPass[SIMPLE_WIFI_PASS_MAX] = '\0';

    if (BOOL__G__FsOk)
    {
        func__Simple_WifiSave();   /* رمزنگاری‌شده روی فلش */
    }
    BOOL__G__WifiRebootPending = true;
    UINT32_T__G__WifiRebootMs = (uint32_t)millis();
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1,\"reboot\":1}");
}

/* ==================== تغییر رمز عبور - /setpass ====================
   قواعد:
   - باید وارد شده باشی.
   - کاربر ساده فقط رمز خودش را می‌تواند عوض کند و رمز فعلی‌اش را هم باید بدهد.
   - ادمین رمز هر کاربری را می‌تواند عوض کند؛ برای رمز خودش رمز فعلی لازم است. */
static void func__Simple_HttpSetPass(void)
{
    String string__user;
    String string__oldPass;
    String string__newPass;
    String string__newPass2;
    bool bool__isAdmin;
    bool bool__isSelf;

    if (!func__Simple_SessionValid())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"login\"}");
        return;
    }
    if (!func__Simple_GetArg("user", string__user) || !func__Simple_GetArg("new", string__newPass))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"user/new\"}");
        return;
    }
    (void)func__Simple_GetArg("old", string__oldPass);
    (void)func__Simple_GetArg("new2", string__newPass2);

    if (string__newPass.length() < SIMPLE_USER_PASS_MIN || string__newPass.length() > SIMPLE_USER_PASS_MAX)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"pass 4-32\"}");
        return;
    }
    if (string__newPass2.length() > 0u && string__newPass2 != string__newPass)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"mismatch\"}");
        return;
    }

    bool__isAdmin = (UINT8_T__G__CurrentRole == 1u);
    bool__isSelf = (strcmp(CHAR__G__CurrentUser, string__user.c_str()) == 0);

    if (!bool__isAdmin && !bool__isSelf)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"self\"}");
        return;
    }
    /* برای رمز خودت رمز فعلی لازم است؛ ادمین برای کاربر دیگر نمی‌خواهد */
    if (bool__isSelf)
    {
        if (!func__Simple_VerifyUserPass(string__user.c_str(), string__oldPass.c_str()))
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"old\"}");
            return;
        }
    }

    if (!func__Simple_SetUserPass(string__user.c_str(), string__newPass.c_str()))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"user\"}");
        return;
    }
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/* ==================== پاک‌سازی تاریخچه - فقط ادمین ==================== */
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
    if (string__type == "charge")
    {
        func__Simple_ChargeHistoryClear();
    }
    else if (string__type == "charts")
    {
        func__Simple_ChartsClear();
    }
    else if (string__type == "diag")
    {
        func__Simple_DiagClear();
    }
    else
    {
        func__Simple_ChargeHistoryClear();
        func__Simple_ChartsClear();
        func__Simple_DiagClear();
    }
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/* ==================== ریست دستی قفل‌ها - فقط ادمین ==================== */
/**
 * @brief  درخواست ریست قفل را در صف می‌گذارد (POST /latch/reset?n=5|6).
 *         سناریو ۵: شمارنده‌های رویداد/سیکل (۲۰۰ و ۲۰۱) و در آخر پرچم قفل
 *         (۲۰۲) صفر می‌شوند تا بین فریم‌ها دوباره فعال نشود؛ سناریو ۶:
 *         ماسک حکم باتری خراب (۲۰۳) صفر می‌شود.
 * @return JSON با ok؛ صف پر → کد ۴۰۹ با e=1، ورودی بد → ۴۰۰ با e=2
 */
static void func__Simple_HttpLatchReset(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }

    String string__n;
    bool bool__ok = false;

    if (func__Simple_GetArg("n", string__n))
    {
        int32_t int32_t__n = (int32_t)string__n.toInt();

        if (int32_t__n == 5)
        {
            /* اول شمارنده‌ها صفر شوند؛ قفل آخر پاک شود تا کنترل‌گر بین فریم‌ها
               با آستانهٔ قدیمی آن را دوباره فعال نکند. */
            bool__ok = func__Simple_QueueSlot(SIMPLE_SLOT_EVENTS_ID, 0u);
            bool__ok = (func__Simple_QueueSlot(SIMPLE_SLOT_CYCLES_ID, 0u) == true) && bool__ok;
            bool__ok = (func__Simple_QueueSlot(SIMPLE_SLOT_LATCH_ID, 0u) == true) && bool__ok;
        }
        else if (int32_t__n == 6)
        {
            bool__ok = func__Simple_QueueSlot(SIMPLE_SLOT_DEAD_MASK_ID, 0u);
        }
        else if (int32_t__n == 8)
        {
            bool__ok = func__Simple_QueueSlot(SIMPLE_SLOT_WEAK_RESET_ID, 0u);
        }
        else
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":2}");
            return;
        }
    }
    else
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":2}");
        return;
    }

    char char__A__Out[48];
    (void)snprintf(char__A__Out, sizeof(char__A__Out), "{\"ok\":%u,\"n\":%s}",
                   bool__ok ? 1u : 0u, string__n.c_str());
    SIMPLE_WEB_SERVER_T__G__Server.send(bool__ok ? 200 : 409, "application/json", char__A__Out);
}

/**
 * @brief  وضعیت صف و آخرین پژواک برد (GET /latch) - فقط ادمین.
 *         پنل ‎rep/nak‎ را با خط پایه، ‎id/val‎ را با هدف و ‎q‎ را با صفر
 *         مقایسه می‌کند؛ سناریوی ۵ هم تا سه پژواک نرسد موفق اعلام نمی‌شود.
 * @return JSON با id/val/known/rep/req/nak/busy/q
 */
static void func__Simple_HttpLatchState(void)
{
    uint8_t uint8_t__i;
    uint8_t uint8_t__pending = 0u;
    char char__A__Out[160];

    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }

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

/* ==================== تاریخچهٔ وضعیت (لاگ روزانه) ====================
   دستور کاربر ۲۰۲۶-۱۰-۲۳: پنل ساده هم مثل پنل فنی تاریخچهٔ وضعیت خودش را
   نشان می‌دهد و دانلود می‌کند. متن ردیف‌ها اینجا ساخته می‌شود، نه روی پشته. */
static char CHAR__G__LogTailBuffer[3600];

/* GET /evt : آخرین ردیف‌های یک روز (یا جدیدترین روز) به‌صورت متن */
static void func__Simple_HttpEventLog(void)
{
    uint32_t uint32_t__day = 0u;
    String string__day;

    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    if (func__Simple_GetArg("day", string__day))
    {
        uint32_t__day = (uint32_t)string__day.toInt();
    }
    (void)func__SimpleLog_Tail(CHAR__G__LogTailBuffer, sizeof(CHAR__G__LogTailBuffer),
                               uint32_t__day, SIMPLE_LOG_TAIL_LINES);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/plain", CHAR__G__LogTailBuffer);
}

/* GET /evt/days : روزهای ذخیره‌شده، قدیمی‌تر اول */
static void func__Simple_HttpEventDays(void)
{
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    (void)func__SimpleLog_Days(CHAR__G__LogTailBuffer, sizeof(CHAR__G__LogTailBuffer));
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/plain", CHAR__G__LogTailBuffer);
}

/* GET /evt/dl?day=YYYYMMDD : دانلود یک روز */
static void func__Simple_HttpEventDownload(void)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    char char__ptr_name[64];
    uint32_t uint32_t__day = 0u;
    String string__day;

    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    if (func__Simple_GetArg("day", string__day))
    {
        uint32_t__day = (uint32_t)string__day.toInt();
    }
    if (uint32_t__day == 0u)
    {
        if (UINT8_T__G__LogIndexCount == 0u)
        {
            SIMPLE_WEB_SERVER_T__G__Server.send(404, "text/plain", "empty");
            return;
        }
        uint32_t__day = UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount - 1u];
    }

    func__SimpleLog_PathOf(char__ptr_path, uint32_t__day);
    if ((BOOL__G__FsOk == false) || (LittleFS.exists(char__ptr_path) == false))
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
    /* خط‌به‌خط فرستاده می‌شود: فایل روز می‌تواند بزرگ‌تر از رم آزاد باشد، پس
       یک‌جا خوانده نمی‌شود. */
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "text/csv", "");
    while (file__day.available())
    {
        String string__line = file__day.readStringUntil('\n');
        if (string__line.length() > 0u)
        {
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(string__line);
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("\n");
    }
    file__day.close();
    SIMPLE_WEB_SERVER_T__G__Server.sendContent("");
}

/* POST /evt/clr : پاک کردن همهٔ روزها (فقط ادمین) */
static void func__Simple_HttpEventClear(void)
{
    uint8_t uint8_t__removed;

    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    if (BOOL__G__FsOk == false)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0}");
        return;
    }
    uint8_t__removed = func__SimpleLog_Clear();
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json",
                                        String("{\"ok\":1,\"removed\":") +
                                        String((unsigned int)uint8_t__removed) + "}");
}
