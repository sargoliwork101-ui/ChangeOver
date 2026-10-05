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
    /* [EN] The whole panel leaves in one send_P. It has grown from 118 KB to
     *      roughly 160 KB with no ceiling anywhere, and on an ESP8266 a page
     *      this size is sent as ~110 consecutive TCP writes: if the link
     *      stalls, the transfer is cut and the browser renders whatever
     *      arrived. The document ends with the settings sub-pages and then
     *      the scripts, so a cut tail looks exactly like "the scenarios
     *      section does not come up" while the earlier cards still show.
     *      This ceiling makes that growth a build error instead of a field
     *      report. Raising it is a deliberate act, not an accident.
     * [FA] کل پنل با یک send_P می‌رود. از ۱۱۸ کیلوبایت به حدود ۱۶۰ رسیده و
     *      هیچ سقفی نداشت؛ روی ESP8266 چنین صفحه‌ای با حدود ۱۱۰ نوشتن پشت سر
     *      هم TCP می‌رود و اگر لینک گیر کند، انتقال بریده می‌شود و مرورگر هر
     *      چه رسیده را نشان می‌دهد. انتهای سند زیرصفحه‌های تنظیمات و بعد
     *      اسکریپت‌هاست، پس بریدگی دقیقاً شبیه «سناریوها بالا نمی‌آید» دیده
     *      می‌شود در حالی که کارت‌های قبلی هستند. این سقف چنین رشدی را به
     *      خطای بیلد تبدیل می‌کند نه گزارش میدانی.
     */
    static_assert(sizeof(ESP_PANEL_HTML) <= ESP_PANEL_HTML_MAX_BYTES,
                   "panel HTML exceeds the transfer budget - split it or raise "
                   "ESP_PANEL_HTML_MAX_BYTES on purpose");

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
 * @brief  [EN] GET /t : compact JSON snapshot {on,age,seq,fl,n,q,q2,q3,q4,ka,t[25],p[108]} (v1.17).
 *              t = TLM u32 fields in spec order (offset 4..80); p = applied params or null.
 *         [FA] مسیر GET /t : خلاصه JSON فشرده {on,age,seq,fl,n,q,q2,q3,q4,ka,t[25],p[108]} (نسخه ۱.۱۷).
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
    uint32_t uint32_t__pendingMask4 = 0u;
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
    static_assert(ESP_PARAM_COUNT <= 128,
                   "pending masks cover ids 0..127; add a fifth word");
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
            else
            {
                uint32_t__pendingMask4 |= (1UL << (uint8_t__index - 96u));
            }
        }
    }

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
        /* [EN] vm/ce: link health. vm > 0 means the STM32 and this panel were
           flashed out of step - the failure that used to be indistinguishable
           from a dead cable. ce counts CRC rejections, so a noisy harness is
           measurable instead of just feeling flaky.
           [FA] vm/ce: سلامت لینک. vm بزرگ‌تر از صفر یعنی STM32 و این پنل ناهماهنگ
           فلش شده‌اند - خرابی‌ای که قبلاً از کابل قطع قابل تشخیص نبود. ce خطاهای
           CRC را می‌شمارد تا هارنس نویزی قابل اندازه‌گیری باشد. */
        "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"fl2\":%u,\"n\":%lu,\"q\":%lu,\"q2\":%lu,\"q3\":%lu,\"q4\":%lu,\"ka\":%lu,\"vm\":%lu,\"ce\":%lu,\"t\":[",
        bool__online ? 1u : 0u, (unsigned long)uint32_t__ageMs, (unsigned int)UINT16_T__G__TlmSeq,
        (unsigned int)UINT8_T__G__TlmFlags, (unsigned int)UINT8_T__G__TlmFlags2, (unsigned long)UINT32_T__G__TlmFrameCount,
        (unsigned long)uint32_t__pendingMask, (unsigned long)uint32_t__pendingMask2,
        (unsigned long)uint32_t__pendingMask3, (unsigned long)uint32_t__pendingMask4,
        (unsigned long)uint32_t__keepaliveAgeMs,
        (unsigned long)UINT32_T__G__RxVersionMismatch,
        (unsigned long)UINT32_T__G__RxCrcError);

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
       [FA] نسخه ۱.۴: هر اندازهٔ median ۱..۱۵ مجاز است (زوج هم)، پس اینجا گرد نمی‌شود. */

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

/* ==================== v1.66 direct LUT push / ارسال مستقیم جدول ==================== */

/* [EN] The staging arrays live in plink_link.h (not on the stack, and the
   paced sender needs them after this handler has returned).
   [FA] آرایه‌های چیدن در plink_link.h هستند: نه روی پشته، و فرستندهٔ
   گام‌به‌گام بعد از بازگشت این هندلر هم به آن‌ها نیاز دارد. */

/**
 * @brief  [EN] POST /lut : push one bench table to the STM32.
 *              Body is plain CSV of unsigned integers, in this exact order:
 *                n1, n2, (chain,power) x n1, (chain,power) x n2, crc32
 *              where crc32 is the browser's reflected CRC32 over
 *              [n1][8 bytes LE per ch1 point][n2][8 bytes LE per ch2 point].
 *              The board recomputes that CRC from what it actually received
 *              and refuses the commit when it differs - so a corrupted push
 *              is rejected instead of silently calibrating the charger
 *              wrongly. A channel may be sent as 0 points (left alone).
 *         [FA] مسیر POST /lut : ارسال یک جدول بنچ به STM32. بدنه CSV عددی با
 *              همین ترتیب است و crc32 همان CRC32 بازتابیدهٔ مرورگر روی
 *              بایت‌های جدول. برد همان CRC را از «آنچه واقعاً گرفته» دوباره
 *              حساب می‌کند و اگر فرق داشت کامیت را رد می‌کند - پس ارسال خراب
 *              رد می‌شود نه اینکه بی‌صدا شارژر را غلط کالیبره کند.
 * @return [EN] None / [FA] ندارد
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
        uint32_t *uint32_t__ptr_chain = (uint8_t__channel == 0u) ? UINT32_T__G__LutChain1 : UINT32_T__G__LutChain2;
        uint32_t *uint32_t__ptr_power = (uint8_t__channel == 0u) ? UINT32_T__G__LutPower1 : UINT32_T__G__LutPower2;
        uint32_t uint32_t__index;

        for (uint32_t__index = 0u; uint32_t__index < UINT32_T__A__Head[uint8_t__channel]; uint32_t__index++)
        {
            if ((!scan.next(&uint32_t__ptr_chain[uint32_t__index])) ||
                (!scan.next(&uint32_t__ptr_power[uint32_t__index])))
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
                    (uint32_t__ptr_power[uint32_t__index] < uint32_t__ptr_power[uint32_t__index - 1u]))
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
 * @brief  [EN] GET /lut : the last handshake the board sent.
 *              {st: stage 1..4, s: status 0=OK, n1, n2: points now active on
 *              the board, crc: CRC32 the board stored, sent: CRC32 we asked
 *              for, age: ms since the ACK, n: ACK count since the push,
 *              tx: step still being sent (0 = sender idle), txe: 0 none,
 *              1 = no answer after the retries, 2 = the board refused a step}.
 *         [FA] مسیر GET /lut : آخرین دست‌دادن برد.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpLutStatus(void)
{
    uint32_t uint32_t__ageMs = (uint32_t)millis() - UINT32_T__G__LutAckMs;

    (void)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
                   "{\"st\":%u,\"s\":%u,\"n1\":%u,\"n2\":%u,\"crc\":%lu,\"sent\":%lu,\"age\":%lu,\"n\":%lu,\"tx\":%u,\"txe\":%u}",
                   (unsigned int)UINT8_T__G__LutAckStage, (unsigned int)UINT8_T__G__LutAckStatus,
                   (unsigned int)UINT8_T__G__LutAckPoints1, (unsigned int)UINT8_T__G__LutAckPoints2,
                   (unsigned long)UINT32_T__G__LutAckCrc32, (unsigned long)UINT32_T__G__LutSentCrc32,
                   (unsigned long)((UINT32_T__G__LutAckCount == 0u) ? 0u : uint32_t__ageMs),
                   (unsigned long)UINT32_T__G__LutAckCount,
                   (unsigned int)UINT8_T__G__LutTxStage, (unsigned int)UINT8_T__G__LutTxError);
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/**
 * @brief  [EN] POST /lut/reset : ask the board to reboot so every module
 *              starts from the table it just stored. Refused unless the last
 *              commit handshake actually succeeded - a reboot is never
 *              offered as a way to "try again".
 *         [FA] مسیر POST /lut/reset : درخواست ریست برد تا همهٔ ماژول‌ها با
 *              جدول تازه شروع کنند. تا وقتی دست‌دادن کامیت موفق نبوده رد
 *              می‌شود - ریست هرگز راهِ «دوباره امتحان کن» نیست.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpLutReset(void)
{
    if ((UINT8_T__G__LutTxStage != 0u) || (UINT8_T__G__LutTxError != 0u) ||
        (UINT8_T__G__LutAckStage != 3u) || (UINT8_T__G__LutAckStatus != 0u) ||
        (UINT32_T__G__LutAckCrc32 != UINT32_T__G__LutSentCrc32) || (UINT32_T__G__LutSentCrc32 == 0u))
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
