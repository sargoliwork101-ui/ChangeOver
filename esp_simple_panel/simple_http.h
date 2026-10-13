/* simple_http.h - هندلرهای HTTP پنل سه صفحه‌ای
   فقط فارسی - نمودار، کاربر، تنظیمات، دیاگ، ساعت، احراز هویت */

/* ==================== کمک پارسر عدد ==================== */
static bool func__Simple_ParseInt(const char *char__ptr_text, int32_t *int32_t__ptr_value)
{
    bool bool__negative = (char__ptr_text[0] == '-');
    uint8_t uint8_t__index = bool__negative ? 1u : 0u;
    uint8_t uint8_t__digits = 0u;
    int32_t int32_t__value = 0;
    while (char__ptr_text[uint8_t__index] != '\0')
    {
        char char__digit = char__ptr_text[uint8_t__index];
        if ((char__digit < '0') || (char__digit > '9') || (uint8_t__digits >= 10u))
        {
            return false;
        }
        int32_t__value = int32_t__value * 10 + (int32_t)(char__digit - '0');
        uint8_t__digits++;
        uint8_t__index++;
    }
    if (uint8_t__digits == 0u) return false;
    *int32_t__ptr_value = bool__negative ? -int32_t__value : int32_t__value;
    return true;
}

/* ==================== صفحه اصلی ==================== */
static void func__Simple_HttpRoot(void)
{
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

/* ==================== وضعیت ورودی با هیسترزیس - دقیقاً مثل برد UI ==================== */
static bool BOOL__G__SimpleInputPresentState = false;
static bool BOOL__G__SimpleInputPresentInit = false;
static bool BOOL__G__SimpleInputOV = false;

static uint8_t func__Simple_InputPresentWithHyst(void)
{
    uint32_t uint32_t__vIn = 0u;
    bool bool__gpioPresent = false;
    if (SIMPLE_LINK_TLM_FIELD_COUNT > 14u)
    {
        uint32_t__vIn = UINT32_T__G__TlmField[14];
    }
    bool__gpioPresent = (UINT8_T__G__TlmFlags & 0x02u) != 0u;

    /* اضافه ولتاژ ۲۸/۲۷ ولت */
    if (BOOL__G__SimpleInputOV == false)
    {
        if (uint32_t__vIn > 28000u) BOOL__G__SimpleInputOV = true;
    }
    else
    {
        if (uint32_t__vIn <= 27000u) BOOL__G__SimpleInputOV = false;
    }

    /* حضور ورودی ۲۱/۲۰ ولت با هیسترزیس - مثل UI */
    if (BOOL__G__SimpleInputPresentInit == false)
    {
        if (uint32_t__vIn >= 21000u) BOOL__G__SimpleInputPresentState = true;
        else if (uint32_t__vIn <= 20000u) BOOL__G__SimpleInputPresentState = false;
        else BOOL__G__SimpleInputPresentState = bool__gpioPresent;
        BOOL__G__SimpleInputPresentInit = true;
    }
    else
    {
        if (uint32_t__vIn > 0u)
        {
            if (BOOL__G__SimpleInputPresentState == false && uint32_t__vIn >= 21000u)
                BOOL__G__SimpleInputPresentState = true;
            else if (BOOL__G__SimpleInputPresentState == true && uint32_t__vIn <= 20000u)
                BOOL__G__SimpleInputPresentState = false;
        }
        else
        {
            if (bool__gpioPresent == false) BOOL__G__SimpleInputPresentState = false;
        }
    }
    return BOOL__G__SimpleInputPresentState ? 1u : 0u;
}

/* ==================== تله‌متری زنده ==================== */
static void func__Simple_HttpTelemetry(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__ageMs = uint32_t__nowMs - UINT32_T__G__LastTlmMs;
    bool bool__online = BOOL__G__TlmSeen && (uint32_t__ageMs <= SIMPLE_LINK_TIMEOUT_MS);
    uint8_t uint8_t__index;
    size_t size_t__used;

    UINT32_T__G__LastBrowserPollMs = uint32_t__nowMs;
    func__Simple_ChartTick();

    uint8_t uint8_t__inputPresent = func__Simple_InputPresentWithHyst();

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, SIMPLE_JSON_BUFFER_SIZE,
        "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"fl2\":%u,\"n\":%lu,\"ce\":%lu,\"vm\":%lu,\"inputPresent\":%u,\"user\":\"%s\",\"role\":%u,\"theme\":%u,\"t\":[",
        bool__online ? 1u : 0u,
        (unsigned long)uint32_t__ageMs,
        (unsigned int)UINT16_T__G__TlmSeq,
        (unsigned int)UINT8_T__G__TlmFlags,
        (unsigned int)UINT8_T__G__TlmFlags2,
        (unsigned long)UINT32_T__G__TlmFrameCount,
        (unsigned long)UINT32_T__G__RxCrcError,
        (unsigned long)UINT32_T__G__RxVersionMismatch,
        (unsigned int)uint8_t__inputPresent,
        CHAR__G__CurrentUser,
        (unsigned int)UINT8_T__G__CurrentRole,
        (unsigned int)UINT8_T__G__Theme);

    for (uint8_t__index = 0u; uint8_t__index < SIMPLE_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used],
            SIMPLE_JSON_BUFFER_SIZE - size_t__used,
            "%s%lu", char__ptr_sep, (unsigned long)UINT32_T__G__TlmField[uint8_t__index]);
    }
    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used],
        SIMPLE_JSON_BUFFER_SIZE - size_t__used, "]}");

    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/* ==================== داده نمودارها - بهینه حافظه با ارسال تکه‌ای ==================== */
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
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"time\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart3M_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart3M_Index + SIMPLE_CHART_3M_SIZE - UINT8_T__G__Chart3M_Count + uint8_t__i) % SIMPLE_CHART_3M_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart3M_Time[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("]}");
    }
    else /* 1d */
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
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"time\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__Chart1D_Count; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - UINT8_T__G__Chart1D_Count + uint8_t__i) % SIMPLE_CHART_1D_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__Chart1D_Time[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vin\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__ChartCount; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__ChartIndex + SIMPLE_CHART_HISTORY_SIZE - UINT8_T__G__ChartCount + uint8_t__i) % SIMPLE_CHART_HISTORY_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__ChartVin[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("],\"vpack\":[");
        for (uint8_t__i = 0u; uint8_t__i < UINT8_T__G__ChartCount; uint8_t__i++)
        {
            uint8_t__idx = (uint8_t)((UINT8_T__G__ChartIndex + SIMPLE_CHART_HISTORY_SIZE - UINT8_T__G__ChartCount + uint8_t__i) % SIMPLE_CHART_HISTORY_SIZE);
            if (uint8_t__i != 0u) SIMPLE_WEB_SERVER_T__G__Server.sendContent(",");
            SIMPLE_WEB_SERVER_T__G__Server.sendContent(String(UINT32_T__G__ChartVpack[uint8_t__idx]));
        }
        SIMPLE_WEB_SERVER_T__G__Server.sendContent("]}");
    }
}

/* ==================== دیاگ - بهینه حافظه با ارسال تکه‌ای ==================== */
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

/* ==================== همگام‌سازی ساعت ==================== */
static void func__Simple_HttpTimeSync(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }

    if (!SIMPLE_WEB_SERVER_T__G__Server.hasArg("ts"))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"ts\"}");
        return;
    }

    int32_t int32_t__ts = 0;
    if (!func__Simple_ParseInt(SIMPLE_WEB_SERVER_T__G__Server.arg("ts").c_str(), &int32_t__ts) || int32_t__ts <= 0)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"bad\"}");
        return;
    }

    func__Simple_SendTimeSync((uint32_t)int32_t__ts);
    UINT32_T__G__BoardTimeSec = (uint32_t)int32_t__ts;

    if (BOOL__G__FsOk)
    {
        File file__f = LittleFS.open(SIMPLE_TIME_FILE, "w");
        if (file__f)
        {
            file__f.print("{\"ts\":");
            file__f.print(int32_t__ts);
            file__f.print("}");
            file__f.close();
        }
    }

    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/* ==================== لاگین ==================== */
static void func__Simple_HttpLogin(void)
{
    if (!SIMPLE_WEB_SERVER_T__G__Server.hasArg("user") || !SIMPLE_WEB_SERVER_T__G__Server.hasArg("pass"))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }

    String string__user = SIMPLE_WEB_SERVER_T__G__Server.arg("user");
    String string__pass = SIMPLE_WEB_SERVER_T__G__Server.arg("pass");

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

/* ==================== افزودن کاربر ==================== */
static void func__Simple_HttpAddUser(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    if (!SIMPLE_WEB_SERVER_T__G__Server.hasArg("user") || !SIMPLE_WEB_SERVER_T__G__Server.hasArg("pass") || !SIMPLE_WEB_SERVER_T__G__Server.hasArg("role"))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }
    String string__user = SIMPLE_WEB_SERVER_T__G__Server.arg("user");
    String string__pass = SIMPLE_WEB_SERVER_T__G__Server.arg("pass");
    int32_t int32_t__role = 0;
    func__Simple_ParseInt(SIMPLE_WEB_SERVER_T__G__Server.arg("role").c_str(), &int32_t__role);
    if (func__Simple_AddUser(string__user.c_str(), string__pass.c_str(), (uint8_t)int32_t__role))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
    }
    else
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"full\"}");
    }
}

/* ==================== حذف کاربر ==================== */
static void func__Simple_HttpDelUser(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    if (!SIMPLE_WEB_SERVER_T__G__Server.hasArg("user"))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }
    String string__user = SIMPLE_WEB_SERVER_T__G__Server.arg("user");
    if (func__Simple_DeleteUser(string__user.c_str()))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
    }
    else
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
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
    if (!SIMPLE_WEB_SERVER_T__G__Server.hasArg("theme"))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }
    int32_t int32_t__theme = 0;
    func__Simple_ParseInt(SIMPLE_WEB_SERVER_T__G__Server.arg("theme").c_str(), &int32_t__theme);
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

/* ==================== وای‌فای - تنظیم - فقط ادمین ==================== */
static void func__Simple_HttpSetWifi(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    if (!SIMPLE_WEB_SERVER_T__G__Server.hasArg("ssid") || !SIMPLE_WEB_SERVER_T__G__Server.hasArg("pass"))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"ssid/pass\"}");
        return;
    }
    String string__ssid = SIMPLE_WEB_SERVER_T__G__Server.arg("ssid");
    String string__pass = SIMPLE_WEB_SERVER_T__G__Server.arg("pass");
    if (string__ssid.length() < 1u || string__ssid.length() > SIMPLE_WIFI_SSID_MAX) { SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"ssid len\"}"); return; }
    if (string__pass.length() < 8u || string__pass.length() > SIMPLE_WIFI_PASS_MAX) { SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"pass len 8-32\"}"); return; }

    (void)strncpy(CHAR__G__WifiSsid, string__ssid.c_str(), SIMPLE_WIFI_SSID_MAX);
    CHAR__G__WifiSsid[SIMPLE_WIFI_SSID_MAX] = '\0';
    (void)strncpy(CHAR__G__WifiPass, string__pass.c_str(), SIMPLE_WIFI_PASS_MAX);
    CHAR__G__WifiPass[SIMPLE_WIFI_PASS_MAX] = '\0';

    if (BOOL__G__FsOk)
    {
        File file__f = LittleFS.open(SIMPLE_WIFI_FILE, "w");
        if (file__f)
        {
            file__f.print("{\"ssid\":\"");
            file__f.print(CHAR__G__WifiSsid);
            file__f.print("\",\"pass\":\"");
            file__f.print(CHAR__G__WifiPass);
            file__f.print("\"}");
            file__f.close();
        }
    }
    BOOL__G__WifiRebootPending = true;
    UINT32_T__G__WifiRebootMs = (uint32_t)millis();
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1,\"reboot\":1}");
}

/* ==================== پاک‌سازی تاریخچه - فقط ادمین ==================== */
static void func__Simple_HttpClearHistory(void)
{
    if (!func__Simple_IsAdmin())
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
        return;
    }
    String string__type = SIMPLE_WEB_SERVER_T__G__Server.hasArg("type") ? SIMPLE_WEB_SERVER_T__G__Server.arg("type") : "all";
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
    else /* all */
    {
        func__Simple_ChargeHistoryClear();
        func__Simple_ChartsClear();
        func__Simple_DiagClear();
    }
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}


