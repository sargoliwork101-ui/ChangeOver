/* simple_http.h - هندلرهای HTTP پنل ساده
   فقط فارسی - فقط / و /t */

/* ==================== هندلر صفحه اصلی ==================== */
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

/* ==================== هندلر تله‌متری ==================== */
static void func__Simple_HttpTelemetry(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__ageMs = uint32_t__nowMs - UINT32_T__G__LastTlmMs;
    bool bool__online = BOOL__G__TlmSeen && (uint32_t__ageMs <= SIMPLE_LINK_TIMEOUT_MS);
    uint8_t uint8_t__index;
    size_t size_t__used;

    UINT32_T__G__LastBrowserPollMs = uint32_t__nowMs;

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, SIMPLE_JSON_BUFFER_SIZE,
        "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"fl2\":%u,\"n\":%lu,\"ce\":%lu,\"vm\":%lu,\"inputPresent\":%u,\"t\":[",
        bool__online ? 1u : 0u,
        (unsigned long)uint32_t__ageMs,
        (unsigned int)UINT16_T__G__TlmSeq,
        (unsigned int)UINT8_T__G__TlmFlags,
        (unsigned int)UINT8_T__G__TlmFlags2,
        (unsigned long)UINT32_T__G__TlmFrameCount,
        (unsigned long)UINT32_T__G__RxCrcError,
        (unsigned long)UINT32_T__G__RxVersionMismatch,
        (UINT8_T__G__TlmFlags & 0x02u) ? 1u : 0u);

    for (uint8_t__index = 0u; uint8_t__index < SIMPLE_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used],
            SIMPLE_JSON_BUFFER_SIZE - size_t__used,
            "%s%lu", char__ptr_sep, (unsigned long)UINT32_T__G__TlmField[uint8_t__index]);
    }

    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used],
        SIMPLE_JSON_BUFFER_SIZE - size_t__used, "]}");

    (void)size_t__used;
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}
