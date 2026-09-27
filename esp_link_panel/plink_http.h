/* plink_http.h - HTTP handlers: / /t /s /m /benchlog (+info).
   Included by esp_link_panel.ino (single translation unit, order matters).
   No include guard on purpose: including twice would redefine everything. */
/* ==================== HTTP Handlers ==================== */

/**
 * @brief  [EN] Strict decimal parse of an HTTP argument: optional '-', 1..7 digits, nothing else.
 *              (String::toInt() returns 0 for garbage, which would silently target ID 0 / value 0.)
 *         [FA] تبدیل سخت‌گیرانهٔ آرگومان HTTP به عدد: '-' اختیاری و ۱ تا ۷ رقم، بدون هیچ چیز دیگر.
 *              (toInt() برای ورودی خراب صفر می‌دهد و بی‌صدا شناسه/مقدار صفر را هدف می‌گیرد.)
 * @param  char__ptr_text      [EN] NUL-terminated text / [FA] متن پایان‌یافته با NUL
 * @param  int32_t__ptr_value  [EN] Output, -9999999..9999999 / [FA] خروجی، -۹۹۹۹۹۹۹ تا ۹۹۹۹۹۹۹
 * @return [EN] true when the text is a valid integer / [FA] true اگر متن عدد صحیح معتبر باشد
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
        if ((char__digit < '0') || (char__digit > '9') || (uint8_t__digits >= 7u))
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
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpRoot(void)
{
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send_P(200, "text/html", ESP_PANEL_HTML);
}

/**
 * @brief  [EN] GET /f.css : Vazirmatn @font-face (cached one year by the browser).
 *         [FA] مسیر GET /f.css : فونت وزیرمتن (مرورگر یک سال کش می‌کند).
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpFont(void)
{
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", ESP_HTTP_FONT_CACHE);
    ESP_WEB_SERVER_T__G__Server.send_P(200, "text/css", ESP_PANEL_FONT_CSS);
}

/**
 * @brief  [EN] GET /t : compact JSON snapshot {on,age,seq,fl,n,q,q2,q3,ka,t[20],p[77]} (v1.16).
 *              t = TLM u32 fields in spec order (offset 4..80); p = applied params or null.
 *         [FA] مسیر GET /t : خلاصه JSON فشرده {on,age,seq,fl,n,q,q2,q3,ka,t[20],p[77]} (نسخه ۱.۱۶).
 *              t فیلدهای u32 تله‌متری به ترتیب سند (آفست ۴ تا ۸۰)؛ p مقدار اعمال‌شده یا null.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpTelemetry(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__ageMs = uint32_t__nowMs - UINT32_T__G__LastTlmMs;
    bool bool__online = BOOL__G__TlmSeen && (uint32_t__ageMs <= ESP_LINK_TIMEOUT_MS);
    uint32_t uint32_t__pendingMask = 0u;
    uint32_t uint32_t__pendingMask2 = 0u;
    uint32_t uint32_t__pendingMask3 = 0u;
    uint32_t uint32_t__keepaliveAgeMs = uint32_t__nowMs - UINT32_T__G__LastKeepaliveMs;
    uint8_t uint8_t__index;
    size_t size_t__used;

    UINT32_T__G__LastBrowserPollMs = uint32_t__nowMs;
    BOOL__G__BrowserSeen = true;

    /* [EN] v1.16: 77 params need three u32 masks (and 1UL << 32+ is UB),
            so ids 0..31 go to "q", 32..63 to "q2" and 64..76 to "q3"
            (panel apend() reads all three).
       [FA] نسخه ۱.۱۶: ۷۷ پارامتر سه ماسک u32 می‌خواهد (و شیفت ۳۲+ تعریف‌نشده
            است)، پس شناسه‌های ۰..۳۱ در q و ۳۲..۶۳ در q2 و ۶۴..۷۶ در q3 می‌روند. */
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
            else
            {
                uint32_t__pendingMask3 |= (1UL << (uint8_t__index - 64u));
            }
        }
    }

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
        "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"n\":%lu,\"q\":%lu,\"q2\":%lu,\"q3\":%lu,\"ka\":%lu,\"t\":[",
        bool__online ? 1u : 0u, (unsigned long)uint32_t__ageMs, (unsigned int)UINT16_T__G__TlmSeq,
        (unsigned int)UINT8_T__G__TlmFlags, (unsigned long)UINT32_T__G__TlmFrameCount,
        (unsigned long)uint32_t__pendingMask, (unsigned long)uint32_t__pendingMask2,
        (unsigned long)uint32_t__pendingMask3,
        (unsigned long)uint32_t__keepaliveAgeMs);

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
            /* [EN] Signed IDs 4..6 are two's complement on the wire / [FA] شناسه‌های ۴ تا ۶ مکمل دو هستند */
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
 *         [FA] مسیر POST /s?id=&v= : محدودسازی و صف کردن یک SET_PARAM (آخرین مقدار معتبر است).
 * @return [EN] None / [FA] ندارد
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
       [FA] نسخه ۱.۴: هر اندازهٔ مدین ۱..۱۵ مجاز است (زوج هم)، پس اینجا گرد نمی‌شود. */

    UINT32_T__G__TxParamValue[uint8_t__id] = (uint32_t)int32_t__clamped;
    BOOL__G__TxParamPending[uint8_t__id] = true;
    BOOL__G__ParamUserSet[uint8_t__id] = true;
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/**
 * @brief  [EN] POST /m : restart the bench statistics window and queue one GET_PARAMS (spec 5.6 v2).
 *              v1.7: the wizard resets this when the DMM form OPENS and reads it when the user
 *              PRESSES submit, so the logged window is the moment of the typed meters.
 *         [FA] مسیر POST /m : شروع دوبارهٔ پنجرهٔ آمار بنچ و صف کردن یک GET_PARAMS (بخش 5.6 نسخه ۲).
 *              نسخهٔ ۱.۷: ویزارد با باز شدن فرم مولتی‌متر این را صفر می‌کند و همان لحظهٔ زدن «ثبت»
 *              می‌خواند تا پنجرهٔ ثبتشده هم‌لحظهِ عددهای واردشدهٔ کاربر باشد.
 * @return [EN] None / [FA] ندارد
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
 *         [FA] مسیر GET /m : {n، s[20] مجموع، lo[20]، hi[20]، la[20] آخرین فریم، or خطاها، seq، fl} (مرورگر s را بر n تقسیم می‌کند).
 * @return [EN] None / [FA] ندارد
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
 * @return [EN] Size in bytes / [FA] اندازه به بایت
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
 *         [FA] مسیر GET /benchlog : دانلود CSV (خالی = فقط عنوان ستون‌ها)؛ GET /benchlog?i=1 : {fs,size,max}.
 * @return [EN] None / [FA] ندارد
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

/**
 * @brief  [EN] POST /benchlog/add (text/plain body): append panel-built CSV line(s).
 *              400 = empty/too long/not newline-terminated/non-printable, 503 = no file system,
 *              507 = the ~100 KB cap would be exceeded (nothing written).
 *         [FA] مسیر POST /benchlog/add (بدنهٔ متنی): افزودن خط(های) CSV ساختهٔ پنل.
 *              400 = خالی/خیلی بلند/بدون خط جدید پایانی/نویسهٔ غیرقابل چاپ، 503 = فایل‌سیستم نیست،
 *              507 = از سقف حدود ۱۰۰KB می‌گذرد (چیزی نوشته نمی‌شود).
 * @return [EN] None / [FA] ندارد
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
 *         [FA] مسیر POST /benchlog/clear : حذف CSV (افزودن بعدی آن را با سطر عنوان از نو می‌سازد).
 * @return [EN] None / [FA] ندارد
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
