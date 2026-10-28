/* plink_http.h - HTTP handlers: / /t /s /m /benchlog (+info).
   Included by ESP_Calibration.ino (single translation unit, order matters).
   No include guard on purpose: including twice would redefine everything. */
/* ==================== HTTP Handlers ==================== */

/**
 * @brief  [EN] Strict decimal parse of an HTTP argument: optional '-', 1..9 digits, nothing else.
 *              (String::toInt() returns 0 for garbage, which would silently target ID 0 / value 0.)
 *         [FA] تبدیل سخت‌گیرانهٔ آرگومان HTTP به عدد: '-' اختیاری و ۱ تا ۹ رقم، بدون هیچ چیز دیگر.
 *              (toInt() برای ورودی خراب صفر می‌دهد و بی‌صدا شناسه/مقدار صفر را هدف می‌گیرد.)
 * @param  char__ptr_text      [EN] NUL-terminated text / [FA]‎ متن پایان‌یافته با NUL
 * @param  int32_t__ptr_value  [EN] Output, -999999999..999999999 / [FA]‎ خروجی، -۹۹۹۹۹۹۹۹۹ تا ۹۹۹۹۹۹۹۹۹
 * @return [EN] true when the text is a valid integer / [FA] true‎ اگر متن عدد صحیح معتبر باشد
 */
static bool func__Esp_ParseInt(const char *char__ptr_text, int32_t *int32_t__ptr_value)
{
    bool bool__negative = (char__ptr_text[0] == '-');
    uint8_t uint8_t__index = bool__negative ? 1u : 0u;
    uint8_t uint8_t__digits = 0u;
    int32_t int32_t__value = 0;

    while (char__ptr_text[uint8_t__index] != '\0')
    {
        char char__digit = char__ptr_text[uint8_t__index];
        if ((char__digit < '0') || (char__digit > '9') || (uint8_t__digits >= 9u))
        {
            return false;
        }
        int32_t int32_t__digitValue = (int32_t)(char__digit - '0');
        int32_t int32_t__shifted = int32_t__value * 10;
        int32_t__value = int32_t__shifted + int32_t__digitValue;
        uint8_t__digits++;
        uint8_t__index++;
    }

    if (uint8_t__digits == 0u)
    {
        return false;
    }

    *int32_t__ptr_value = bool__negative ? -int32_t__value : int32_t__value;
    return true;
}

/**
 * @brief  [EN] GET / : serve the web panel from flash (never cached, so panel
 *              updates reach every browser immediately).
 *         [FA] مسیر GET / : ارسال پنل وب از حافظه فلش (بدون کش تا هر آپدیت پنل
 *              بلافاصله به همهٔ مرورگرها برسد).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpRoot(void)
{
    /* [EN] Do not hand the whole ~384 KiB document to one send_P call. The
     *      ESP8266 can write that PROGMEM string, but a stalled client then
     *      loses the tail of the response; the tail contains the settings
     *      cards and scripts, so the symptom is exactly "the simulator/ESP
     *      panel never comes up" while the first cards may still appear.
     *      Use HTTP chunked streaming with small PROGMEM slices. This keeps
     *      the loop/yield path in the WebServer implementation active and
     *      makes a slow phone/browser recover instead of receiving a partial
     *      document. The ceiling remains a compile-time guard on flash size.
     * [FA] کل سند حدود ۳۸۴ کیلوبایت است و نباید با یک فراخوانی send_P فرستاده
     *      شود. اگر کلاینت کند یا لینک گیر کند، انتهای پاسخ که کارت‌های
     *      تنظیمات و اسکریپت‌ها را دارد از بین می‌رود و نتیجه دقیقاً شبیه
     *      «شبیه‌ساز/پنل روی ESP بالا نمی‌آید» دیده می‌شود. حالا پاسخ با HTTP
     *      chunked و تکه‌های کوچک از PROGMEM فرستاده می‌شود تا مسیر yield وب‌سرور
     *      فعال بماند و سند ناقص تحویل نشود. سقف اندازه همچنان نگهبان بیلد است.
     */
    static_assert(sizeof(ESP_PANEL_HTML) <= ESP_PANEL_HTML_MAX_BYTES,
                   "panel HTML exceeds the transfer budget - split it or raise "
                   "ESP_PANEL_HTML_MAX_BYTES on purpose");

    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    ESP_WEB_SERVER_T__G__Server.send(200, "text/html", "");
    static const size_t SIZE_T__PANEL_CHUNK = 2048u;
    const size_t SIZE_T__PANEL_BYTES = sizeof(ESP_PANEL_HTML) - 1u;
    for (size_t SIZE_T__OFFSET = 0u;
         SIZE_T__OFFSET < SIZE_T__PANEL_BYTES;
         SIZE_T__OFFSET += SIZE_T__PANEL_CHUNK)
    {
        const size_t SIZE_T__REMAINING = SIZE_T__PANEL_BYTES - SIZE_T__OFFSET;
        const size_t SIZE_T__COUNT = (SIZE_T__REMAINING < SIZE_T__PANEL_CHUNK)
                                   ? SIZE_T__REMAINING : SIZE_T__PANEL_CHUNK;
        ESP_WEB_SERVER_T__G__Server.sendContent_P(ESP_PANEL_HTML + SIZE_T__OFFSET,
                                                   SIZE_T__COUNT);
        /* [EN] Keep the UART parser fed while the page streams: without this
           the RX ring overflows on slow clients and the panel shows the
           yellow "rejected by CRC" warning after every page load.
           [FA] حین ارسال صفحه، پارسر UART را تغذیه نگه دار: بدون این، روی
           کلاینت کند حلقهٔ RX سرریز می‌کند و پنل بعد از هر بارگذاری صفحه
           هشدار زرد «رد شده با CRC» می‌دهد. */
        func__Esp_DrainSerial();
        /* [EN] Feed the watchdog between slices. Half a megabyte over a slow
                phone takes seconds; without this the board resets in the middle
                of the document and the browser shows NOTHING - the exact
                "the page never comes up" report.
           [FA] بین تکه‌ها نگهبان را تغذیه کن. نیم مگابایت روی گوشی کند چند
                ثانیه طول می‌کشد؛ بدون این، برد وسط سند ری‌استارت می‌شود و
                مرورگر «هیچ» نشان نمی‌دهد - همان «صفحه بالا نمی‌آید». */
        yield();
    }
}

/**
 * @brief  [EN] GET /ping : a few bytes that prove the web server is alive even
 *              when the big page cannot be delivered. The user's first check
 *              when "the page does not come up": if /ping answers and / does
 *              not, the board is fine and the panel document is the problem;
 *              if /ping is silent too, the board is not reachable at all.
 *         [FA] چند بایت که ثابت می‌کند وب‌سرور زنده است، حتی وقتی صفحهٔ بزرگ
 *              نمی‌رسد. اولین بررسی کاربر وقتی «صفحه بالا نمی‌آید»: اگر
 *              ‎/ping‎ جواب بدهد و ‎/‎ نه، برد سالم است و خود سند پنل مشکل
 *              دارد؛ اگر ‎/ping‎ هم ساکت باشد، برد اصلاً در دسترس نیست.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpPing(void)
{
    char char__text[64];

    (void)snprintf(char__text, sizeof(char__text), "pong fs=%u log=%u rows=%u",
                   (unsigned int)((BOOL__G__FsOk != false) ? 1u : 0u),
                   (unsigned int)UINT8_T__G__LogWriteFault,
                   (unsigned int)UINT16_T__G__LogRowsToday);
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send(200, "text/plain", char__text);
}

/**
 * @brief  [EN] GET /f.css : Vazirmatn @font-face (cached one year by the browser).
 *         [FA] مسیر ‎GET /f.css‎ : فونت وزیرمتن (مرورگر یک سال کش می‌کند).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpFont(void)
{
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", ESP_HTTP_FONT_CACHE);
    ESP_WEB_SERVER_T__G__Server.send_P(200, "text/css", ESP_PANEL_FONT_CSS);
}

/**
 * @brief  [EN] GET /t : compact JSON snapshot {on,age,seq,fl,n,q,q2,q3,q4,q5,ka,t[],p[]} (v1.17).
 *              t = TLM u32 fields in spec order (offset 4..80); p = applied params or null.
 *         [FA] مسیر ‎GET /t‎ : خلاصه JSON فشرده {on,age,seq,fl,n,q,q2,q3,q4,q5,ka,t[],p[]} (نسخه ۱.۱۷).
 *              t فیلدهای u32 تله‌متری به ترتیب سند (آفست ۴ تا ۸۰)؛ p مقدار اعمال‌شده یا null.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpTelemetry(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__ageMs = uint32_t__nowMs - UINT32_T__G__LastTlmMs;
    bool bool__online = BOOL__G__TlmSeen && (uint32_t__ageMs <= ESP_LINK_TIMEOUT_MS);
    uint32_t uint32_t__pendingMask = 0u;
    uint32_t uint32_t__pendingMask2 = 0u;
    uint32_t uint32_t__pendingMask3 = 0u;
    uint32_t uint32_t__pendingMask4 = 0u;
    /* [EN] v1.80: ids 128..131 (the scenario-6 lamp/buzzer) need a fifth word -
       the assert below had asked for exactly this. [FA] واژهٔ پنجم برای ۱۲۸..۱۳۱. */
    uint32_t uint32_t__pendingMask5 = 0u;
    uint32_t uint32_t__pendingMask6 = 0u; /* [EN] v1.87: ids 160..164 (scenario ticks). / [FA] نسخهٔ ۱٫۸۷: شناسه‌های ۱۶۰..۱۶۴. */
    uint32_t uint32_t__keepaliveAgeMs = uint32_t__nowMs - UINT32_T__G__LastKeepaliveMs;
    uint8_t uint8_t__index;
    size_t size_t__used;

    UINT32_T__G__LastBrowserPollMs = uint32_t__nowMs;
    BOOL__G__BrowserSeen = true;

    /* [EN] v1.16: 77 params need three u32 masks (and 1UL << 32+ is UB),
            so ids 0..31 go to "q", 32..63 to "q2" and 64..95 to "q3".
            v1.23 briefly needed a FOURTH mask because the three-stage PID
            pushed the top id to 97; v1.24 dropped the redundant third gain
            row, the top id is 92 again, and "q4" went with it - but the
            field kept being emitted as a constant 0.
            v1.28 brings it back for real: the user-ordered limits block ends
            at id 107, and the old "else" arm shifted EVERY id >= 64 into
            mask3, so id 96 evaluated 1UL << 32 - undefined behaviour, not a
            wrong pixel. Each arm is now a closed range and the static assert
            below is what actually stops the next person.
       [FA] نسخه ۱.۱۶: ۷۷ پارامتر سه ماسک u32 می‌خواهد (و شیفت ۳۲+ تعریف‌نشده
            است)، پس شناسه‌های ۰..۳۱ در q و ۳۲..۶۳ در q2 و ۶۴..۹۵ در q3.
            نسخهٔ ۱.۲۳ کوتاه‌مدت ماسک چهارم خواست؛ نسخهٔ ۱.۲۴ آن را برداشت ولی
            فیلد همچنان ثابت صفر فرستاده می‌شد.
            نسخهٔ ۱.۲۸ آن را واقعی برمی‌گرداند: بلوک حدها به دستور کاربر تا
            شناسهٔ ۱۰۷ می‌رود و شاخهٔ else قدیمی هر شناسهٔ ۶۴ به بالا را در
            ماسک۳ می‌ریخت، یعنی شناسهٔ ۹۶ می‌شد 1UL << 32 — رفتار تعریف‌نشده،
            نه فقط یک پیکسل غلط. حالا هر شاخه بازهٔ بسته دارد و assert پایین
            چیزی است که واقعاً جلوی نفر بعدی را می‌گیرد. */
    static_assert(ESP_PARAM_COUNT <= 192,
                   "pending masks cover ids 0..191; add a seventh word");
    for (uint8_t__index = 0u; uint8_t__index < ESP_PARAM_COUNT; uint8_t__index++)
    {
        if (BOOL__G__TxParamPending[uint8_t__index])
        {
            if (uint8_t__index < 32u)
            {
                uint32_t__pendingMask |= (1UL << uint8_t__index);
            }
            else if (uint8_t__index < 64u)
            {
                uint32_t__pendingMask2 |= (1UL << (uint8_t__index - 32u));
            }
            else if (uint8_t__index < 96u)
            {
                uint32_t__pendingMask3 |= (1UL << (uint8_t__index - 64u));
            }
            else if (uint8_t__index < 128u)
            {
                uint32_t__pendingMask4 |= (1UL << (uint8_t__index - 96u));
            }
            else if (uint8_t__index < 160u)
            {
                uint32_t__pendingMask5 |= (1UL << (uint8_t__index - 128u));
            }
            else
            {
                uint32_t__pendingMask6 |= (1UL << (uint8_t__index - 160u));
            }
        }
    }

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
        /* [EN] vm/ce: link health. vm > 0 means the STM32 and this panel were
           flashed out of step - the failure that used to be indistinguishable
           from a dead cable. ce counts CRC rejections, so a noisy harness is
           measurable instead of just feeling flaky.
           [‎FA] vm/ce‎: سلامت لینک. vm بزرگ‌تر از صفر یعنی STM32 و این پنل ناهماهنگ
           فلش شده‌اند - خرابی‌ای که قبلاً از کابل قطع قابل تشخیص نبود. ce خطاهای
           CRC را می‌شمارد تا هارنس نویزی قابل اندازه‌گیری باشد. */
        "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"fl2\":%u,\"n\":%lu,\"q\":%lu,\"q2\":%lu,\"q3\":%lu,\"q4\":%lu,\"q5\":%lu,\"q6\":%lu,\"ka\":%lu,\"vm\":%lu,\"ce\":%lu,\"nk\":%lu,\"nr\":%lu,\"nid\":%lu,\"lko\":%u,\"bpv\":%u,\"bfw\":%u,\"bfl\":%u,\"brx\":%lu,\"bce\":%u,\"bvm\":%u,\"bpc\":%u,\"bpr\":%u,\"hac\":%lu,\"hls\":%lu,\"lac\":%lu,\"t\":[",
        bool__online ? 1u : 0u, (unsigned long)uint32_t__ageMs, (unsigned int)UINT16_T__G__TlmSeq,
        (unsigned int)UINT8_T__G__TlmFlags, (unsigned int)UINT8_T__G__TlmFlags2, (unsigned long)UINT32_T__G__TlmFrameCount,
        (unsigned long)uint32_t__pendingMask, (unsigned long)uint32_t__pendingMask2,
        (unsigned long)uint32_t__pendingMask3, (unsigned long)uint32_t__pendingMask4,
        (unsigned long)uint32_t__pendingMask5,
        (unsigned long)uint32_t__pendingMask6,
        (unsigned long)uint32_t__keepaliveAgeMs,
        (unsigned long)UINT32_T__G__RxVersionMismatch,
        (unsigned long)UINT32_T__G__RxCrcError,
        (unsigned long)UINT32_T__G__NakCount, (unsigned long)UINT8_T__G__NakReason,
        (unsigned long)UINT8_T__G__NakId,
        /* [EN] Handshake block (v1.111): lko=confirmed, bpv/bfw=board versions,
           brx/bce/bvm=what the board itself saw on the wire, lac=age of last ACK.
           [FA] بلوک دست‌دادن (v1.111): lko=تأییدشده، ‎bpv/bfw‎=نسخه‌های برد،
           ‎brx/bce/bvm‎=آنچه خود برد روی سیم دید، lac=سن آخرین پاسخ. */
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
        (unsigned long)(BOOL__G__HelloSeen
                        ? (uint32_t__nowMs - UINT32_T__G__LastHelloAckMs)
                        : 0xFFFFFFFFu));

    for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
            "%s%lu", char__ptr_sep, (unsigned long)UINT32_T__G__TlmField[uint8_t__index]);
    }

    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, "],\"p\":[");

    for (uint8_t__index = 0u; uint8_t__index < ESP_PARAM_COUNT; uint8_t__index++)
    {
        const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
        if (BOOL__G__ParamKnown[uint8_t__index])
        {
            /* [‎EN] Signed IDs 4..6 are two's complement on the wire / [FA]‎ شناسه‌های ۴ تا ۶ مکمل دو هستند */
            int32_t int32_t__value = (int32_t)UINT32_T__G__ParamApplied[uint8_t__index];
            size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                "%s%ld", char__ptr_sep, (long)int32_t__value);
        }
        else
        {
            size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                "%snull", char__ptr_sep);
        }
    }

    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, "]}");
    (void)size_t__used;
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/**
 * @brief  [EN] POST /s?id=&v= : clamp and queue one SET_PARAM (latest value wins).
 *         [FA] مسیر ‎POST /s‎?id=&v= : محدودسازی و صف کردن یک SET_PARAM (آخرین مقدار معتبر است).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpSetParam(void)
{
    if ((!ESP_WEB_SERVER_T__G__Server.hasArg("id")) || (!ESP_WEB_SERVER_T__G__Server.hasArg("v")))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }

    int32_t int32_t__id = -1;
    int32_t int32_t__value = 0;
    bool bool__idOk = func__Esp_ParseInt(ESP_WEB_SERVER_T__G__Server.arg("id").c_str(), &int32_t__id);
    bool bool__valueOk = func__Esp_ParseInt(ESP_WEB_SERVER_T__G__Server.arg("v").c_str(), &int32_t__value);

    if ((!bool__idOk) || (!bool__valueOk) || (int32_t__id < 0) || (int32_t__id >= (int32_t)ESP_PARAM_COUNT))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }

    uint8_t uint8_t__id = (uint8_t)int32_t__id;
    int32_t int32_t__min = INT32_T__G__ParamMin[uint8_t__id];
    int32_t int32_t__max = INT32_T__G__ParamMax[uint8_t__id];
    int32_t int32_t__clamped = (int32_t__value < int32_t__min) ? int32_t__min : int32_t__value;
    int32_t__clamped = (int32_t__clamped > int32_t__max) ? int32_t__max : int32_t__clamped;

    /* [EN] v1.4: any median size 1..15 is valid (even sizes too), so no rounding here.
       [FA] نسخه ۱.۴: هر اندازهٔ median ۱..۱۵ مجاز است (زوج هم)، پس اینجا گرد نمی‌شود. */

    UINT32_T__G__TxParamValue[uint8_t__id] = (uint32_t)int32_t__clamped;
    BOOL__G__TxParamPending[uint8_t__id] = true;
    BOOL__G__ParamUserSet[uint8_t__id] = true;
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/* ==================== ریست دستی قفل‌ها (اسلات‌های زمان‌اجرا) ==================== */
/**
 * @brief  یک درخواست ریست را در صف اسلات‌ها می‌گذارد.
 * @param  uint8_t__id شناسهٔ اسلات (۲۰۰..۲۰۳)؛ بیرون این بازه رد می‌شود
 * @param  uint32_t__value مقدار اعمالی؛ برای ریست صفر
 * @return true اگر در صف نشست (یا ادغام شد)، false اگر صف پر بود
 */
static bool func__Esp_QueueSlot(uint8_t uint8_t__id, uint32_t uint32_t__value)
{
    uint8_t uint8_t__index;

    if ((uint8_t__id < ESP_SLOT_FIRST_ID) || (uint8_t__id > ESP_SLOT_LAST_ID))
    {
        return false;
    }

    /* یک درخواست تکراری برای همان شناسه، دوباره صف نمی‌شود؛ مقدارش به‌روز
       می‌شود تا دو کلیک پشت‌سرهم دو فریم بی‌فایده نسازد. */
    for (uint8_t__index = 0u; uint8_t__index < ESP_SLOT_QUEUE_LEN; uint8_t__index++)
    {
        if (BOOL__G__SlotQueuePending[uint8_t__index] &&
            (UINT8_T__G__SlotQueueId[uint8_t__index] == uint8_t__id))
        {
            UINT32_T__G__SlotQueueValue[uint8_t__index] = uint32_t__value;
            return true;
        }
    }

    for (uint8_t__index = 0u; uint8_t__index < ESP_SLOT_QUEUE_LEN; uint8_t__index++)
    {
        if (BOOL__G__SlotQueuePending[uint8_t__index] == false)
        {
            UINT8_T__G__SlotQueueId[uint8_t__index] = uint8_t__id;
            UINT32_T__G__SlotQueueValue[uint8_t__index] = uint32_t__value;
            BOOL__G__SlotQueuePending[uint8_t__index] = true;
            UINT32_T__G__SlotRequestCount++;
            return true;
        }
    }

    UINT8_T__G__SlotBusyReject++;
    return false;
}

/**
 * @brief  دکمهٔ ریست کارت سناریو: ‎POST /latch/reset‎?‎n=5|6|8‎ را جواب می‌دهد.
 *         برای سناریو ۵ ابتدا شمارنده‌های رویداد/سیکل (۲۰۰ و ۲۰۱) و در آخر
 *         پرچم قفل (۲۰۲) صفر می‌شوند تا بین فریم‌ها دوباره فعال نشود؛ برای
 *         سناریو ۶ ماسک حکم باتری خراب (۲۰۳) صفر می‌شود و برای سناریو ۸ اسلات
 *         ۲۰۴ به برد می‌رود تا حکم باتری ضعیف را پاک کند. مقدار صفر = پاک.
 * @param  n شمارهٔ سناریو؛ هر چیزی جز ۵، ۶ یا ۸ رد می‌شود
 * @return JSON با ok؛ صف پر → کد ۴۰۹ با ‎e=1‎، ورودی بد → کد ۴۰۰ با ‎e=2‎
 */
static void func__Esp_HttpLatchReset(void)
{
    int32_t int32_t__n = 0;
    bool bool__nOk = ESP_WEB_SERVER_T__G__Server.hasArg("n") &&
                     func__Esp_ParseInt(ESP_WEB_SERVER_T__G__Server.arg("n").c_str(), &int32_t__n);

    if ((!bool__nOk) || ((int32_t__n != 5) && (int32_t__n != 6) && (int32_t__n != 8)))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":2}");
        return;
    }

    bool bool__queued;

    if (int32_t__n == 5)
    {
        /* اول شمارنده‌ها صفر شوند؛ قفل آخر پاک شود تا کنترل‌گر بین فریم‌ها
           با آستانهٔ قدیمی آن را دوباره فعال نکند. */
        bool__queued = func__Esp_QueueSlot(ESP_SLOT_EVENTS_ID, 0u);
        bool__queued = (func__Esp_QueueSlot(ESP_SLOT_CYCLES_ID, 0u) == true) && bool__queued;
        bool__queued = (func__Esp_QueueSlot(ESP_SLOT_LATCH_ID, 0u) == true) && bool__queued;
    }
    else if (int32_t__n == 6)
    {
        bool__queued = func__Esp_QueueSlot(ESP_SLOT_DEAD_MASK_ID, 0u);
    }
    else
    {
        /* [EN] v1.86: scenario 8 - the STM drops its weak-battery verdict. */
        bool__queued = func__Esp_QueueSlot(ESP_SLOT_WEAK_RESET_ID, 0u);
    }

    char char__A__Out[48];
    (void)snprintf(char__A__Out, sizeof(char__A__Out), "{\"ok\":%u,\"n\":%ld}",
                   bool__queued ? 1u : 0u, (long)int32_t__n);
    ESP_WEB_SERVER_T__G__Server.send(bool__queued ? 200 : 409, "application/json", char__A__Out);
}

/**
 * @brief  وضعیت صف و آخرین پژواک برد: ‎GET /latch‎.
 *         پنل از ‎rep/nak‎ نسبت به خط پایه، شناسه/مقدار آخر و صف باقی‌مانده
 *         می‌فهمد ریست هدف واقعاً نشسته یا برد آن را رد کرده است.
 * @return JSON با ‎id/val/known/rep/req/nak/busy/q‎
 */
static void func__Esp_HttpLatchState(void)
{
    uint8_t uint8_t__index;
    uint8_t uint8_t__pending = 0u;

    for (uint8_t__index = 0u; uint8_t__index < ESP_SLOT_QUEUE_LEN; uint8_t__index++)
    {
        if (BOOL__G__SlotQueuePending[uint8_t__index])
        {
            uint8_t__pending++;
        }
    }

    char char__A__Out[160];
    (void)snprintf(char__A__Out, sizeof(char__A__Out),
                   "{\"ok\":1,\"id\":%u,\"val\":%lu,\"known\":%u,\"rep\":%lu,"
                   "\"req\":%lu,\"nak\":%lu,\"busy\":%u,\"q\":%u}",
                   (unsigned)UINT8_T__G__SlotLastId,
                   (unsigned long)UINT32_T__G__SlotLastValue,
                   BOOL__G__SlotLastKnown ? 1u : 0u,
                   (unsigned long)UINT32_T__G__SlotReportCount,
                   (unsigned long)UINT32_T__G__SlotRequestCount,
                   (unsigned long)UINT32_T__G__SlotNakCount,
                   (unsigned)UINT8_T__G__SlotBusyReject,
                   (unsigned)uint8_t__pending);
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", char__A__Out);
}

/**
 * @brief  [EN] POST /m : restart the bench statistics window and queue one GET_PARAMS (spec 4).
 *              v1.7: the wizard resets this when the DMM form OPENS and reads it when the user
 *              PRESSES submit, so the logged window is the moment of the typed meters.
 *         [FA] مسیر ‎POST /m‎ : شروع دوبارهٔ پنجرهٔ آمار بنچ و صف کردن یک GET_PARAMS (بخش 4 نسخه ۲).
 *              نسخهٔ ۱.۷: ویزارد با باز شدن فرم مولتی‌متر این را صفر می‌کند و همان لحظهٔ زدن «ثبت»
 *              می‌خواند تا پنجرهٔ ثبتشده هم‌لحظهِ عددهای واردشدهٔ کاربر باشد.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpStatReset(void)
{
    uint8_t uint8_t__index;

    for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        UINT32_T__G__StatSum[uint8_t__index] = 0u;
        UINT32_T__G__StatMin[uint8_t__index] = 0u;
        UINT32_T__G__StatMax[uint8_t__index] = 0u;
        UINT32_T__G__StatLast[uint8_t__index] = 0u;
    }

    UINT32_T__G__StatFaultOr = 0u;
    UINT16_T__G__StatSeq = 0u;
    UINT8_T__G__StatFlags = 0u;
    UINT32_T__G__StatCount = 0u;
    BOOL__G__TxGetPending = true;
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/**
 * @brief  [EN] GET /m : {n, s[20] sums, lo[20], hi[20], la[20] last frame, or faults OR, seq, fl} (browser divides s by n).
 *         [FA] مسیر ‎GET /m : {n‎، s[20] مجموع، lo[20]، hi[20]، la[20] آخرین فریم، or خطاها، seq، fl} (مرورگر s را بر n تقسیم می‌کند).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpStatRead(void)
{
    const uint32_t *UINT32_T__A__Table[4] = { UINT32_T__G__StatSum, UINT32_T__G__StatMin, UINT32_T__G__StatMax, UINT32_T__G__StatLast };
    const char *CHAR__A__Key[4] = { "s", "lo", "hi", "la" };
    uint8_t uint8_t__table;
    uint8_t uint8_t__index;
    size_t size_t__used;

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE, "{\"n\":%lu",
                                    (unsigned long)UINT32_T__G__StatCount);

    for (uint8_t__table = 0u; uint8_t__table < 4u; uint8_t__table++)
    {
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                                         ",\"%s\":[", CHAR__A__Key[uint8_t__table]);
        for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
        {
            const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
            size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                                             "%s%lu", char__ptr_sep,
                                             (unsigned long)UINT32_T__A__Table[uint8_t__table][uint8_t__index]);
        }
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, "]");
    }

    (void)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, ",\"or\":%lu,\"seq\":%u,\"fl\":%u}",
                   (unsigned long)UINT32_T__G__StatFaultOr, (unsigned int)UINT16_T__G__StatSeq, (unsigned int)UINT8_T__G__StatFlags);
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/**
 * @brief  [EN] Current size of the bench log file (0 when missing or no file system).
 *         [FA] اندازهٔ فعلی فایل ثبت بنچ (بدون فایل یا فایل‌سیستم = ۰).
 * @return [EN] Size in bytes / [FA]‎ اندازه به بایت
 */
static uint32_t func__Esp_BenchLogSize(void)
{
    uint32_t uint32_t__size = 0u;

    if (BOOL__G__FsOk && LittleFS.exists(ESP_BENCHLOG_PATH))
    {
        File file__log = LittleFS.open(ESP_BENCHLOG_PATH, "r");
        if (file__log)
        {
            uint32_t__size = (uint32_t)file__log.size();
            file__log.close();
        }
    }

    return uint32_t__size;
}

/**
 * @brief  [EN] GET /benchlog : download the CSV (header only when empty); GET /benchlog?i=1 : {fs,size,max}.
 *         [FA] مسیر ‎GET /benchlog‎ : دانلود CSV (خالی = فقط عنوان ستون‌ها)؛ ‎GET /benchlog‎?‎i=1 : {fs,size,max}‎.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpBenchLogGet(void)
{
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");

    if (ESP_WEB_SERVER_T__G__Server.hasArg("i"))
    {
        (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE, "{\"fs\":%u,\"size\":%lu,\"max\":%lu}",
                       BOOL__G__FsOk ? 1u : 0u, (unsigned long)func__Esp_BenchLogSize(),
                       (unsigned long)ESP_BENCHLOG_MAX_BYTES);
        ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
        return;
    }

    ESP_WEB_SERVER_T__G__Server.sendHeader("Content-Disposition", "attachment; filename=benchlog.csv");
    if (func__Esp_BenchLogSize() == 0u)
    {
        ESP_WEB_SERVER_T__G__Server.send(200, "text/csv", ESP_BENCHLOG_HEADER);
        return;
    }

    File file__log = LittleFS.open(ESP_BENCHLOG_PATH, "r");
    if (!file__log)
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "text/plain", "fs");
        return;
    }
    (void)ESP_WEB_SERVER_T__G__Server.streamFile(file__log, "text/csv");
    file__log.close();
}

/* ==================== v1.66 direct LUT push / ارسال مستقیم جدول ==================== */

/* [EN] The staging arrays live in plink_link.h (not on the stack, and the
   paced sender needs them after this handler has returned).
   [FA] آرایه‌های چیدن در plink_link.h هستند: نه روی پشته، و فرستندهٔ
   گام‌به‌گام بعد از بازگشت این هندلر هم به آن‌ها نیاز دارد. */

/**
 * @brief  [EN] POST /lut : push one bench table to the STM32.
 *              Body is plain CSV of unsigned integers, in this exact order:
 *                n1, n2, (chain,mA) x n1, (chain,mA) x n2, crc32
 *              where crc32 is the browser's reflected CRC32 over
 *              [n1][8 bytes LE per ch1 point][n2][8 bytes LE per ch2 point].
 *              The board recomputes that CRC from what it actually received
 *              and refuses the commit when it differs - so a corrupted push
 *              is rejected instead of silently calibrating the charger
 *              wrongly. A channel may be sent as 0 points; that is a valid
 *              command which removes that channel's flash override and
 *              returns it to the compiled table.
 *         [FA] مسیر ‎POST /lut‎ : ارسال یک جدول بنچ به STM32. بدنه CSV عددی با
 *              همین ترتیب است و crc32 همان CRC32 بازتابیدهٔ مرورگر روی
 *              بایت‌های جدول. برد همان CRC را از «آنچه واقعاً گرفته» دوباره
 *              حساب می‌کند و اگر فرق داشت کامیت را رد می‌کند - پس ارسال خراب
 *              رد می‌شود نه اینکه بی‌صدا شارژر را غلط کالیبره کند. صفر نقطه
 *              نیز معتبر است و override فلش آن کانال را حذف می‌کند تا جدول
 *              کامپایل‌شده دوباره مرجع شود.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpLutPush(void)
{
    const String string__body = ESP_WEB_SERVER_T__G__Server.arg("plain");
    const uint32_t uint32_t__len = (uint32_t)string__body.length();
    uint32_t uint32_t__cursor = 0u;
    uint32_t UINT32_T__A__Head[2] = { 0u, 0u };
    uint32_t uint32_t__crc32 = 0u;
    uint8_t uint8_t__channel;
    uint8_t uint8_t__head;

    /* [EN] Strict CSV scanner: digits and separators only, every field a
       u32. String::toInt() would turn garbage into a silent zero, which on
       a calibration table is the worst possible failure mode.
       [FA] اسکنر سخت‌گیر CSV: فقط رقم و جداکننده. toInt() ورودی خراب را
       بی‌صدا صفر می‌کند و روی جدول کالیبراسیون بدترین حالت ممکن است. */
    struct Scan
    {
        const String *s;
        uint32_t len;
        uint32_t *cur;
        bool next(uint32_t *out)
        {
            uint32_t value = 0u;
            uint32_t digits = 0u;
            while ((*cur < len) && (((*s)[*cur] == ',') || ((*s)[*cur] == ' ') ||
                                    ((*s)[*cur] == '\n') || ((*s)[*cur] == '\r')))
            {
                (*cur)++;
            }
            while ((*cur < len) && ((*s)[*cur] >= '0') && ((*s)[*cur] <= '9'))
            {
                if (digits >= 10u)
                {
                    return false;
                }
                value = (value * 10u) + (uint32_t)((*s)[*cur] - '0');
                digits++;
                (*cur)++;
            }
            if (digits == 0u)
            {
                return false;
            }
            *out = value;
            return true;
        }
    };
    Scan scan = { &string__body, uint32_t__len, &uint32_t__cursor };

    if ((uint32_t__len == 0u) || (uint32_t__len > 1600u))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"len\"}");
        return;
    }

    for (uint8_t__head = 0u; uint8_t__head < 2u; uint8_t__head++)
    {
        if ((!scan.next(&UINT32_T__A__Head[uint8_t__head])) ||
            (UINT32_T__A__Head[uint8_t__head] > (uint32_t)ESP_LUT_POINTS_MAX) ||
            (UINT32_T__A__Head[uint8_t__head] == 1u))
        {
            ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"n\"}");
            return;
        }
    }
    if ((UINT32_T__A__Head[0] == 0u) && (UINT32_T__A__Head[1] == 0u))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"empty\"}");
        return;
    }

    for (uint8_t__channel = 0u; uint8_t__channel < 2u; uint8_t__channel++)
    {
        uint32_t *uint32_t__ptr_chain = (uint8_t__channel == 0u) ? UINT32_T__G__LutCounts1 : UINT32_T__G__LutCounts2;
        uint32_t *uint32_t__ptr_battery = (uint8_t__channel == 0u) ? UINT32_T__G__LutMa1 : UINT32_T__G__LutMa2;
        uint32_t uint32_t__index;

        for (uint32_t__index = 0u; uint32_t__index < UINT32_T__A__Head[uint8_t__channel]; uint32_t__index++)
        {
            if ((!scan.next(&uint32_t__ptr_chain[uint32_t__index])) ||
                (!scan.next(&uint32_t__ptr_battery[uint32_t__index])))
            {
                ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"pt\"}");
                return;
            }
            /* [EN] Same legality the board enforces, checked here too so the
               operator sees the reason immediately instead of a status code.
               [FA] همان قانونی که برد اجرا می‌کند، اینجا هم بررسی می‌شود تا
               کاربر دلیل را فوری ببیند نه یک کد وضعیت. */
            if (uint32_t__index > 0u)
            {
                if ((uint32_t__ptr_chain[uint32_t__index] <= uint32_t__ptr_chain[uint32_t__index - 1u]) ||
                    (uint32_t__ptr_battery[uint32_t__index] < uint32_t__ptr_battery[uint32_t__index - 1u]))
                {
                    ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"mono\"}");
                    return;
                }
            }
        }
    }

    if (!scan.next(&uint32_t__crc32))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"crc\"}");
        return;
    }

    /* [EN] Clear the previous handshake BEFORE sending, so the browser can
       never read a stale ACK as the answer to this push.
       [FA] پاک‌کردن دست‌دادن قبلی «قبل از» ارسال، تا مرورگر ACK کهنه را پاسخ
       این ارسال نخواند. */
    UINT8_T__G__LutAckStage = 0u;
    UINT8_T__G__LutAckStatus = 0u;
    UINT32_T__G__LutAckCrc32 = 0u;
    UINT32_T__G__LutAckCount = 0u;

    /* [EN] v1.67: stage it; the link layer releases one frame per ACK so the
       board's 256-byte RX ring can never be overrun (finding L1).
       [FA] نسخه ۱.۶۷: فقط چیده می‌شود؛ لایهٔ لینک هر فریم را پس از تأیید
       می‌فرستد تا حلقهٔ ۲۵۶ بایتی گیرندهٔ برد سرریز نکند (یافتهٔ L1). */
    func__Esp_LutTxStart((uint8_t)UINT32_T__A__Head[0], (uint8_t)UINT32_T__A__Head[1],
                         uint32_t__crc32);

    (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
                   "{\"ok\":1,\"n1\":%lu,\"n2\":%lu,\"crc\":%lu}",
                   (unsigned long)UINT32_T__A__Head[0], (unsigned long)UINT32_T__A__Head[1],
                   (unsigned long)uint32_t__crc32);
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/**
 * @brief  [EN] GET /lut : return the push handshake and the point-by-point
 *              active-table readback. The readback arrays are emitted only
 *              after both channels arrive; CRC/count fields remain metadata.
 *         [FA] مسیر ‎GET /lut‎ دست‌دادن ارسال و بازخوانی عددبه‌عدد جدول فعال
 *              را برمی‌گرداند. آرایه‌ها فقط پس از رسیدن هر دو کانال می‌آیند؛
 *              CRC/تعداد فقط metadata هستند.
 * @‎return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpLutStatus(void)
{
    uint32_t uint32_t__ackAgeMs = (uint32_t)millis() - UINT32_T__G__LutAckMs;
    uint32_t uint32_t__readAgeMs = (uint32_t)millis() - UINT32_T__G__LutReadMs;
    uint8_t uint8_t__count1 = BOOL__G__LutReadReady ? UINT8_T__G__LutReadCount1 : 0u;
    uint8_t uint8_t__count2 = BOOL__G__LutReadReady ? UINT8_T__G__LutReadCount2 : 0u;
    size_t size_t__used;
    uint8_t uint8_t__index;

    size_t__used = (size_t)snprintf(
        CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
        "{\"st\":%u,\"s\":%u,\"n1\":%u,\"n2\":%u,\"crc\":%lu,\"sent\":%lu,\"age\":%lu,\"n\":%lu,\"tx\":%u,\"txe\":%u,\"read\":{\"ready\":%u,\"pending\":%u,\"error\":%u,\"age\":%lu,\"n1\":%u,\"n2\":%u,\"r1\":[",
        (unsigned int)UINT8_T__G__LutAckStage, (unsigned int)UINT8_T__G__LutAckStatus,
        (unsigned int)UINT8_T__G__LutAckPoints1, (unsigned int)UINT8_T__G__LutAckPoints2,
        (unsigned long)UINT32_T__G__LutAckCrc32, (unsigned long)UINT32_T__G__LutSentCrc32,
        (unsigned long)((UINT32_T__G__LutAckCount == 0u) ? 0u : uint32_t__ackAgeMs),
        (unsigned long)UINT32_T__G__LutAckCount,
        (unsigned int)UINT8_T__G__LutTxStage, (unsigned int)UINT8_T__G__LutTxError,
        BOOL__G__LutReadReady ? 1u : 0u, BOOL__G__LutReadPending ? 1u : 0u,
        (unsigned int)UINT8_T__G__LutReadError,
        (unsigned long)(BOOL__G__LutReadReady ? uint32_t__readAgeMs : 0u),
        (unsigned int)uint8_t__count1, (unsigned int)uint8_t__count2);

    for (uint8_t__index = 0u; uint8_t__index < uint8_t__count1; uint8_t__index++)
    {
        const char *char__ptr_separator = (uint8_t__index == 0u) ? "" : ",";
        size_t__used += (size_t)snprintf(
            &CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
            "%s[%lu,%lu]", char__ptr_separator,
            (unsigned long)UINT32_T__G__LutReadCounts1[uint8_t__index],
            (unsigned long)UINT32_T__G__LutReadMa1[uint8_t__index]);
    }

    size_t__used += (size_t)snprintf(
        &CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
        "],\"r2\":[");
    for (uint8_t__index = 0u; uint8_t__index < uint8_t__count2; uint8_t__index++)
    {
        const char *char__ptr_separator = (uint8_t__index == 0u) ? "" : ",";
        size_t__used += (size_t)snprintf(
            &CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
            "%s[%lu,%lu]", char__ptr_separator,
            (unsigned long)UINT32_T__G__LutReadCounts2[uint8_t__index],
            (unsigned long)UINT32_T__G__LutReadMa2[uint8_t__index]);
    }

    (void)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                   "]}}");
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/**
 * @brief  [EN] POST /lut/read : ask STM32 for the active point values.
 *         [FA] مسیر ‎POST /lut/read‎ مقدار واقعی نقاط فعال را از STM32 می‌خواهد.
 * @‎return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpLutRead(void)
{
    if (UINT8_T__G__LutTxStage != 0u)
    {
        ESP_WEB_SERVER_T__G__Server.send(409, "application/json", "{\"ok\":0,\"e\":\"busy\"}");
        return;
    }

    if (!BOOL__G__LutReadPending)
    {
        (void)func__Esp_LutReadStart();
    }
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1,\"pending\":1}");
}

/**
 * @brief  [EN] POST /lut/reset : ask the board to reboot so every module
 *              starts from the table it just stored. Refused unless the last
 *              commit handshake and exact point-by-point readback both passed
 *              - a reboot is never offered as a way to "try again".
 *         [FA] مسیر ‎POST /lut/reset‎ : درخواست ریست برد تا همهٔ ماژول‌ها با
 *              جدول تازه شروع کنند. تا وقتی دست‌دادن کامیت و بازخوانی دقیق
 *              عددبه‌عدد موفق نباشد رد می‌شود - ریست راهِ «دوباره امتحان کن» نیست.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpLutReset(void)
{
    if ((UINT8_T__G__LutTxStage != 0u) || (UINT8_T__G__LutTxError != 0u) ||
        (UINT8_T__G__LutAckStage != 3u) || (UINT8_T__G__LutAckStatus != 0u) ||
        (UINT32_T__G__LutAckCrc32 != UINT32_T__G__LutSentCrc32) ||
        (UINT32_T__G__LutSentCrc32 == 0u) || (!func__Esp_LutReadMatchesStaged()))
    {
        ESP_WEB_SERVER_T__G__Server.send(409, "application/json", "{\"ok\":0,\"e\":\"handshake\"}");
        return;
    }

    func__Esp_SendLutReset();
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/**
 * @brief  [EN] POST /benchlog/add (text/plain body): append panel-built CSV line(s).
 *              400 = empty/too long/not newline-terminated/non-printable, 503 = no file system,
 *              507 = the ~100 KB cap would be exceeded (nothing written).
 *         [FA] مسیر ‎POST /benchlog/add (‎بدنهٔ متنی): افزودن خط(های) CSV ساختهٔ پنل.
 *              400 = خالی/خیلی بلند/بدون خط جدید پایانی/نویسهٔ غیرقابل چاپ، 503 = فایل‌سیستم نیست،
 *              507 = از سقف حدود ۱۰۰KB می‌گذرد (چیزی نوشته نمی‌شود).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpBenchLogAdd(void)
{
    const String string__body = ESP_WEB_SERVER_T__G__Server.arg("plain");
    const uint32_t uint32_t__len = (uint32_t)string__body.length();
    uint32_t uint32_t__index;
    uint32_t uint32_t__size;
    uint32_t uint32_t__extra;

    if ((uint32_t__len == 0u) || (uint32_t__len > ESP_BENCHLOG_MAX_POST) || (string__body[uint32_t__len - 1u] != '\n'))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }
    for (uint32_t__index = 0u; uint32_t__index < uint32_t__len; uint32_t__index++)
    {
        const char char__c = string__body[uint32_t__index];
        if ((char__c != '\n') && ((char__c < ' ') || (char__c > '~')))
        {
            ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
            return;
        }
    }
    if (!BOOL__G__FsOk)
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0}");
        return;
    }

    uint32_t__size = func__Esp_BenchLogSize();
    uint32_t__extra = (uint32_t__size == 0u) ? (uint32_t)(sizeof(ESP_BENCHLOG_HEADER) - 1u) : 0u;
    if ((uint32_t__size + uint32_t__extra + uint32_t__len) > ESP_BENCHLOG_MAX_BYTES)
    {
        (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE, "{\"ok\":0,\"full\":1,\"size\":%lu}", (unsigned long)uint32_t__size);
        ESP_WEB_SERVER_T__G__Server.send(507, "application/json", CHAR__G__JsonBuffer);
        return;
    }

    File file__log = LittleFS.open(ESP_BENCHLOG_PATH, "a");
    if (!file__log)
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0}");
        return;
    }
    if (uint32_t__extra != 0u)
    {
        (void)file__log.print(ESP_BENCHLOG_HEADER);
    }
    (void)file__log.print(string__body);
    file__log.close();

    (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE, "{\"ok\":1,\"size\":%lu}", (unsigned long)func__Esp_BenchLogSize());
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/**
 * @brief  [EN] POST /benchlog/clear : delete the CSV (the next append recreates it with the header).
 *         [FA] مسیر ‎POST /benchlog/clear‎ : حذف CSV (افزودن بعدی آن را با سطر عنوان از نو می‌سازد).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpBenchLogClear(void)
{
    if (!BOOL__G__FsOk)
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0}");
        return;
    }
    if (LittleFS.exists(ESP_BENCHLOG_PATH))
    {
        (void)LittleFS.remove(ESP_BENCHLOG_PATH);
    }
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/* ==================== v1.93 state history / تاریخچهٔ وضعیت ==================== */

/* [EN] The tail text is built here, not on the stack: 30 rows x ~80 bytes plus
        the terminator, and the handler must return before anything else runs.
   [FA] متن ردیف‌ها اینجا ساخته می‌شود نه روی پشته: ۳۰ ردیف × ~۸۰ بایت به‌علاوهٔ
        پایان‌بند، و هندلر باید پیش از هر کار دیگری برگردد. */
static char CHAR__G__LogTailBuffer[3600];
static char CHAR__G__LogLostText[8];

/**
 * @brief  [EN] POST /clock?sec=..&tz=.. : adopt the phone clock (or ask it).
 *         [FA] مسیر ‎POST /clock‎ : گرفتن ساعت گوشی (یا پرسیدن آن).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpClockSet(void)
{
    bool bool__ok = false;
    uint32_t uint32_t__sec = 0u;
    int32_t int32_t__tz = 0;

    if (ESP_WEB_SERVER_T__G__Server.hasArg("sec"))
    {
        uint32_t__sec = (uint32_t)ESP_WEB_SERVER_T__G__Server.arg("sec").toInt();
    }
    if (ESP_WEB_SERVER_T__G__Server.hasArg("tz"))
    {
        int32_t__tz = (int32_t)ESP_WEB_SERVER_T__G__Server.arg("tz").toInt();
    }
    bool__ok = func__EspLog_TimeSet(uint32_t__sec, int32_t__tz);
    (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
                   "{\"ok\":%u,\"sec\":%lu}",
                   (bool__ok != false) ? 1u : 0u,
                   (unsigned long)func__EspLog_NowSec());
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/* تفکیک «روز انتخاب‌نشده (خودکار)» از «روز صفر بدون ساعت (00000000)» */
static void func__Esp_ParseDayArg(bool *bool__ptr_hasDay, uint32_t *uint32_t__ptr_day)
{
    *bool__ptr_hasDay = false;
    *uint32_t__ptr_day = ESP_LOG_DAY_AUTO;
    if (ESP_WEB_SERVER_T__G__Server.hasArg("day"))
    {
        String string__day = ESP_WEB_SERVER_T__G__Server.arg("day");
        string__day.trim();
        if (string__day.length() > 0u)
        {
            *bool__ptr_hasDay = true;
            *uint32_t__ptr_day = (uint32_t)strtoul(string__day.c_str(), NULL, 10);
        }
    }
}

/**
 * @brief  [EN] GET /evt[?day=YYYYMMDD] : the last rows of one day as CSV text.
 *         [FA] مسیر ‎GET /evt‎ : آخرین ردیف‎های یک روز به‌صورت متن CSV.
 * @return [EN] None / [FA]‎ ندارد
 */
/* [EN] Where the rows of this answer come from, for the panel banner:
        1 = the flash, 0 = the file system is off, 2 = a flash write failed and
        the newest rows are the ones kept in RAM. Without the third value the
        page could not tell "nothing happened" from "I could not write".
   [FA] ردیف‌های این پاسخ از کجا می‌آیند، برای نوار هشدار پنل:
        ۱ یعنی فلش، ۰ یعنی فایل‌سیستم خاموش، ۲ یعنی نوشتن روی فلش شکست خورد و
        جدیدترین ردیف‌ها در رم‌اند. بدون مقدار سوم، صفحه نمی‌توانست «چیزی
        نشده» را از «نتوانستم بنویسم» جدا کند. */
/* [EN] Rows the RAM ring had to throw away while the flash was down, as text.
        The page shows the number, so a gap in the history is never a mystery.
   [FA] ردیف‌هایی که حلقهٔ رم وقتی فلش خراب بود ناچار شد دور بریزد، به‌صورت
        متن. صفحه همان عدد را نشان می‌دهد تا فاصلهٔ تاریخچه معما نماند. */
static const char *func__Esp_LogLostText(void)
{
    (void)snprintf(CHAR__G__LogLostText, sizeof(CHAR__G__LogLostText), "%u",
                   (unsigned int)UINT16_T__G__LogDroppedRows);
    return CHAR__G__LogLostText;
}

/**
 * @brief  [EN] Format the panel's flash on the user's explicit request and mount
 *              it again. Never called by itself: a format is the only action in
 *              this file that can destroy the history, so it needs a human.
 *         [FA] فرمت کردن فلش پنل به درخواست صریح کاربر و سوار کردن دوبارهٔ آن.
 *              هیچ‌وقت خودبه‌خود صدا زده نمی‌شود: فرمت تنها کاری در این فایل
 *              است که می‌تواند تاریخچه را نابود کند، پس آدم لازم دارد.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpEventFormat(void)
{
    (void)LittleFS.format();
    BOOL__G__FsOk = LittleFS.begin();
    BOOL__G__FsMountFailed = (BOOL__G__FsOk == false);
    if (BOOL__G__FsOk != false)
    {
        func__EspLog_Begin();
    }
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send(200, "text/plain",
                                     (BOOL__G__FsOk != false) ? "ok" : "fail");
}

static const char *func__Esp_LogSourceCode(void)
{
    if (BOOL__G__FsOk == false)
    {
        /* [EN] 0 = no file system at all, 3 = it exists but did not mount, so
                the history on it is untouched and only the newest rows wait in
                RAM.
           [FA] ۰ = فایل‌سیستم نیست، ۳ = هست ولی سوار نشد؛ تاریخچه‌اش
                دست‌نخورده است و فقط چند ردیف آخر در رم منتظرند. */
        return (BOOL__G__FsMountFailed != false) ? "3" : "0";
    }
    if (UINT8_T__G__LogWriteFault != 0u)
    {
        return "2";   /* ESP_LOG_FS_WRITE_FAULT */
    }
    return "1";       /* ESP_LOG_FS_OK */
}

static void func__Esp_HttpEventLog(void)
{
    bool bool__hasDay = false;
    uint32_t uint32_t__day = ESP_LOG_DAY_AUTO;

    ESP_WEB_SERVER_T__G__Server.sendHeader("X-Log-Fs", func__Esp_LogSourceCode());
    ESP_WEB_SERVER_T__G__Server.sendHeader("X-Log-Lost", func__Esp_LogLostText());
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    func__Esp_ParseDayArg(&bool__hasDay, &uint32_t__day);
    if ((bool__hasDay != false) && (uint32_t__day == 0u))
    {
        uint32_t__day = ESP_LOG_DAY_ZERO;
    }
    (void)func__EspLog_Tail(CHAR__G__LogTailBuffer, sizeof(CHAR__G__LogTailBuffer),
                            uint32_t__day, ESP_LOG_TAIL_LINES);
    ESP_WEB_SERVER_T__G__Server.send(200, "text/plain", CHAR__G__LogTailBuffer);
}

/**
 * @brief  [EN] GET /evt/days : saved days, oldest first, space separated.
 *         [FA] مسیر ‎GET /evt/days‎ : روزهای ذخیره‌شده، قدیمی‌تر اول، جداشده با فاصله.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpEventDays(void)
{
    ESP_WEB_SERVER_T__G__Server.sendHeader("X-Log-Fs", func__Esp_LogSourceCode());
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    (void)func__EspLog_Days(CHAR__G__LogTailBuffer, sizeof(CHAR__G__LogTailBuffer));
    ESP_WEB_SERVER_T__G__Server.send(200, "text/plain", CHAR__G__LogTailBuffer);
}

/**
 * @brief  [EN] GET /evt/dl?day=YYYYMMDD : download one day as a CSV attachment.
 *         [FA] مسیر ‎GET /evt/dl‎ : دانلود یک روز به‌صورت فایل CSV.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpEventDownload(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    char char__name[64];
    File file__day;
    bool bool__hasDay = false;
    uint32_t uint32_t__day = ESP_LOG_DAY_AUTO;

    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    func__Esp_ParseDayArg(&bool__hasDay, &uint32_t__day);

    if (BOOL__G__FsOk == false)
    {
        size_t size_t__hdrLen;
        if (UINT8_T__G__LogRamCount == 0u)
        {
            ESP_WEB_SERVER_T__G__Server.send(404, "text/plain", "empty");
            return;
        }
        size_t__hdrLen = strlen(ESP_LOG_HEADER);
        if (size_t__hdrLen >= sizeof(CHAR__G__LogTailBuffer))
        {
            size_t__hdrLen = 0u;
        }
        else
        {
            (void)memcpy(CHAR__G__LogTailBuffer, ESP_LOG_HEADER, size_t__hdrLen);
        }
        (void)func__EspLog_Tail(&CHAR__G__LogTailBuffer[size_t__hdrLen],
                                sizeof(CHAR__G__LogTailBuffer) - size_t__hdrLen,
                                ESP_LOG_DAY_AUTO, ESP_LOG_RAM_ROWS);
        ESP_WEB_SERVER_T__G__Server.sendHeader("Content-Disposition",
                                               "attachment; filename=evt-ram.csv");
        ESP_WEB_SERVER_T__G__Server.send(200, "text/csv", CHAR__G__LogTailBuffer);
        return;
    }

    if (bool__hasDay == false)
    {
        uint32_t__day = func__EspLog_TodayCode();
        func__EspLog_PathOf(char__path, uint32_t__day);
        if (LittleFS.exists(char__path) == false)
        {
            /* [EN] no day file for today yet: the newest day that DOES exist is
                    what the user means by "download it".
               [FA] هنوز فایل امروز ساخته نشده: جدیدترین روزی که واقعاً هست
                    همان است که کاربر می‌خواهد. */
            uint32_t__day = func__EspLog_NewestDay();
            if (uint32_t__day == ESP_LOG_DAY_AUTO)
            {
                ESP_WEB_SERVER_T__G__Server.send(404, "text/plain", "empty");
                return;
            }
        }
    }

    func__EspLog_PathOf(char__path, uint32_t__day);
    if (LittleFS.exists(char__path) == false)
    {
        ESP_WEB_SERVER_T__G__Server.send(404, "text/plain", "missing");
        return;
    }
    (void)snprintf(char__name, sizeof(char__name), "attachment; filename=evt-%08lu.csv",
                   (unsigned long)uint32_t__day);
    ESP_WEB_SERVER_T__G__Server.sendHeader("Content-Disposition", char__name);

    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "text/plain", "fs");
        return;
    }
    (void)ESP_WEB_SERVER_T__G__Server.streamFile(file__day, "text/csv");
    file__day.close();
}

/**
 * @brief  [EN] POST /evt/clr : drop every saved day (the panel asks first).
 *         [FA] مسیر ‎POST /evt/clr‎ : حذف همهٔ روزهای ذخیره‌شده (پنل اول می‌پرسد).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Esp_HttpEventClear(void)
{
    uint8_t uint8_t__removed;

    uint8_t__removed = func__EspLog_Clear();
    (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
                   "{\"ok\":1,\"removed\":%u}", (unsigned int)uint8_t__removed);
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}
