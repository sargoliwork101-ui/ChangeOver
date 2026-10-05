/* plink_link.h - UART frame transmit/receive, TLM store, bench statistics window.
   Included by esp_link_panel.ino (single translation unit, order matters).
   No include guard on purpose: including twice would redefine everything. */
/* ==================== Byte Helpers ==================== */

/**
 * @brief  [EN] Read a little-endian u32 from a byte buffer.
 *         [FA] خواندن عدد u32 اندیان‌کوچک از بافر بایتی.
 * @param  uint8_t__ptr_buffer [EN] Source buffer, at least offset+4 bytes / [FA] بافر مبدا، حداقل offset+4 بایت
 * @param  uint8_t__offset     [EN] Byte offset, 0..108 (payload max 112) / [FA] آفست بایتی، ۰ تا ۱۰۸ (حداکثر payload ۱۱۲)
 * @return [EN] Decoded value / [FA] مقدار رمزگشایی‌شده
 */
static uint32_t func__Esp_ReadU32(const uint8_t *uint8_t__ptr_buffer, uint8_t uint8_t__offset)
{
    uint32_t uint32_t__byte0 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset];
    uint32_t uint32_t__byte1 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset + 1u] << 8;
    uint32_t uint32_t__byte2 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset + 2u] << 16;
    uint32_t uint32_t__byte3 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset + 3u] << 24;
    uint32_t uint32_t__lowHalf = uint32_t__byte0 | uint32_t__byte1;
    uint32_t uint32_t__highHalf = uint32_t__byte2 | uint32_t__byte3;
    return uint32_t__lowHalf | uint32_t__highHalf;
}

/* ==================== Frame Transmit ==================== */

/**
 * @brief  [EN] CRC-16/CCITT-FALSE, identical to the firmware's implementation.
 *              Both sides must agree bit for bit or every frame is rejected.
 *         [FA] CRC-16/CCITT-FALSE، عیناً مثل پیاده‌سازی فرم‌ور. هر دو طرف باید
 *              بیت‌به‌بیت یکی باشند وگرنه هر فریمی رد می‌شود.
 * @param  uint16_t__crc  [EN] Running value / مقدار جاری
 * @param  uint8_t__byte  [EN] Next byte / بایت بعدی
 * @return uint16_t [EN] Updated CRC / CRC به‌روزشده
 */
static uint16_t func__Esp_Crc16(uint16_t uint16_t__crc, uint8_t uint8_t__byte)
{
    uint8_t uint8_t__bit;

    uint16_t__crc = (uint16_t)(uint16_t__crc ^ ((uint16_t)uint8_t__byte << 8));
    for (uint8_t__bit = 0u; uint8_t__bit < 8u; uint8_t__bit++)
    {
        if ((uint16_t__crc & 0x8000u) != 0u)
        {
            uint16_t__crc = (uint16_t)(((uint16_t)(uint16_t__crc << 1)) ^ (uint16_t)ESP_LINK_CRC16_POLY);
        }
        else
        {
            uint16_t__crc = (uint16_t)(uint16_t__crc << 1);
        }
    }
    return uint16_t__crc;
}

/* [EN] Link health, surfaced to the page. A version mismatch means the STM32
   and this panel were flashed out of step - the failure that used to look
   exactly like a dead cable.
   [FA] سلامت لینک، نمایش‌داده‌شده در صفحه. ناهم‌نسخگی یعنی STM32 و این پنل
   ناهماهنگ فلش شده‌اند - خرابی‌ای که قبلاً عیناً شبیه کابل قطع به نظر می‌رسید. */
static uint32_t UINT32_T__G__RxCrcError = 0u;
static uint32_t UINT32_T__G__RxVersionMismatch = 0u;
static uint16_t UINT16_T__G__RxCrc = 0u;
static uint8_t  UINT8_T__G__RxCrcLow = 0u;

/**
 * @brief  [EN] Build and write one frame: AA 55 type len payload xor.
 *         [FA] ساخت و ارسال یک فریم: AA 55 type len payload xor.
 * @param  uint8_t__type        [EN] Message type (0x01 SET_PARAM / 0x02 GET_PARAMS) / [FA] نوع پیام (0x01 یا 0x02)
 * @param  uint8_t__ptr_payload [EN] Payload bytes, may be NULL when len = 0 / [FA] بایت‌های payload؛ برای طول صفر می‌تواند NULL باشد
 * @param  uint16_t__len        [EN] Payload length, 0..512 bytes, u16 LE on the wire (SET frames use 5) / [FA] طول payload، ۰ تا ۵۱۲ بایت (فریم SET پنج بایت است)
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_WriteFrame(uint8_t uint8_t__type, const uint8_t *uint8_t__ptr_payload, uint16_t uint16_t__len)
{
    uint8_t UINT8_T__A__Frame[ESP_LINK_HEADER_SIZE + ESP_LINK_MAX_PAYLOAD + ESP_LINK_CRC_SIZE];
    uint8_t uint8_t__lenLo = (uint8_t)(uint16_t__len & 0xFFu);
    uint8_t uint8_t__lenHi = (uint8_t)((uint16_t__len >> 8) & 0xFFu);
    uint16_t uint16_t__crc = (uint16_t)ESP_LINK_CRC16_INIT;
    uint16_t uint16_t__index;

    if (uint16_t__len > ESP_LINK_MAX_PAYLOAD)
    {
        return;
    }

    UINT8_T__A__Frame[0] = ESP_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = ESP_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = (uint8_t)ESP_LINK_PROTOCOL_VERSION;
    UINT8_T__A__Frame[3] = uint8_t__type;
    UINT8_T__A__Frame[4] = uint8_t__lenLo;
    UINT8_T__A__Frame[5] = uint8_t__lenHi;
    uint16_t__crc = func__Esp_Crc16(uint16_t__crc, (uint8_t)ESP_LINK_PROTOCOL_VERSION);
    uint16_t__crc = func__Esp_Crc16(uint16_t__crc, uint8_t__type);
    uint16_t__crc = func__Esp_Crc16(uint16_t__crc, uint8_t__lenLo);
    uint16_t__crc = func__Esp_Crc16(uint16_t__crc, uint8_t__lenHi);

    for (uint16_t__index = 0u; uint16_t__index < uint16_t__len; uint16_t__index++)
    {
        uint8_t uint8_t__byte = uint8_t__ptr_payload[uint16_t__index];
        UINT8_T__A__Frame[ESP_LINK_HEADER_SIZE + uint16_t__index] = uint8_t__byte;
        uint16_t__crc = func__Esp_Crc16(uint16_t__crc, uint8_t__byte);
    }

    uint16_t uint16_t__crcPosition = (uint16_t)(ESP_LINK_HEADER_SIZE + uint16_t__len);
    UINT8_T__A__Frame[uint16_t__crcPosition] = (uint8_t)(uint16_t__crc & 0xFFu);
    UINT8_T__A__Frame[uint16_t__crcPosition + 1u] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
    uint16_t uint16_t__frameSize = (uint16_t)(uint16_t__crcPosition + ESP_LINK_CRC_SIZE);

    (void)Serial.write(UINT8_T__A__Frame, uint16_t__frameSize);
    UINT32_T__G__LastTxMs = (uint32_t)millis();
    /* [EN] Every valid frame feeds the STM32 dead-man, so it also counts as the keepalive.
       [FA] هر فریم معتبر deadman STM32 را تغذیه می‌کند، پس keepalive هم حساب می‌شود. */
    UINT32_T__G__LastKeepaliveMs = UINT32_T__G__LastTxMs;
}

/**
 * @brief  [EN] Send SET_PARAM [id:u8][value:u32 LE].
 *         [FA] ارسال SET_PARAM با قالب [id:u8][value:u32 LE].
 * @param  uint8_t__id     [EN] Parameter ID, 0..82 (83 params since v1.17) / [FA] شناسه پارامتر، ۰ تا ۸۲ (۸۳ پارامتر از نسخه ۱.۱۷)
 * @param  uint32_t__value [EN] Raw wire value (signed IDs as two's complement) / [FA] مقدار خام (شناسه‌های علامت‌دار به صورت مکمل دو)
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_SendSetParam(uint8_t uint8_t__id, uint32_t uint32_t__value)
{
    uint8_t UINT8_T__A__Payload[ESP_LINK_PARAM_ITEM_SIZE];
    UINT8_T__A__Payload[0] = uint8_t__id;
    UINT8_T__A__Payload[1] = (uint8_t)(uint32_t__value & 0xFFu);
    UINT8_T__A__Payload[2] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
    UINT8_T__A__Payload[3] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
    UINT8_T__A__Payload[4] = (uint8_t)((uint32_t__value >> 24) & 0xFFu);
    func__Esp_WriteFrame(ESP_MSG_SET_PARAM, UINT8_T__A__Payload, ESP_LINK_PARAM_ITEM_SIZE);
}

/**
 * @brief  [EN] Send at most one queued command per ESP_LINK_TX_INTERVAL_MS
 *              (priority order UINT8_T__G__TxOrder), plus the manual-mode keepalive or, with no
 *              browser, the manual-exit request (ID 19 = 0). Never blocks.
 *         [FA] ارسال حداکثر یک فرمان صف‌شده در هر ESP_LINK_TX_INTERVAL_MS
 *              (به ترتیب اولویت UINT8_T__G__TxOrder) و keepalive مود دستی یا، بدون مرورگر، درخواست
 *              خروج از مود دستی (ID 19 = 0). هیچ‌وقت مسدود نمی‌کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_PumpTx(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__elapsedMs = uint32_t__nowMs - UINT32_T__G__LastTxMs;
    uint8_t uint8_t__step;

    if (uint32_t__elapsedMs < ESP_LINK_TX_INTERVAL_MS)
    {
        return;
    }

    for (uint8_t__step = 0u; uint8_t__step < ESP_PARAM_COUNT; uint8_t__step++)
    {
        uint8_t uint8_t__id = UINT8_T__G__TxOrder[uint8_t__step];
        if (BOOL__G__TxParamPending[uint8_t__id])
        {
            BOOL__G__TxParamPending[uint8_t__id] = false;
            func__Esp_SendSetParam(uint8_t__id, UINT32_T__G__TxParamValue[uint8_t__id]);
            return;
        }
    }

    /* [EN] Manual-mode keepalive: every ESP_LINK_KEEPALIVE_MS while manual is active (b5 or param 19),
            whether or not a browser is polling (spec 5.2: from every tab and in the background).
       [FA] keepalive مود دستی: هر ESP_LINK_KEEPALIVE_MS تا وقتی مود دستی فعال است (b5 یا پارامتر ۱۹)،
            چه مرورگری poll کند چه نه (بخش 5.2 سند: از هر تب و در پس‌زمینه). */
    bool bool__flagManual = ((UINT8_T__G__TlmFlags & ESP_TLM_FLAG_MANUAL_MODE) != 0u);
    bool bool__paramManual = BOOL__G__ParamKnown[ESP_PARAM_MANUAL_TEST_MODE] &&
                             (UINT32_T__G__ParamApplied[ESP_PARAM_MANUAL_TEST_MODE] != 0u);
    bool bool__manualActive = bool__flagManual || bool__paramManual;

    /* [EN] Closed-panel guard: no /t poll for ESP_LINK_BROWSER_LOST_MS -> the keepalive is replaced by
            SET ID 19 = 0, repeated every ESP_LINK_KEEPALIVE_MS until the STM32 reports manual off
            (a lost frame is simply retried). A throttled background tab still polls about once per
            second and is not affected.
       [FA] محافظ پنل بسته: اگر ESP_LINK_BROWSER_LOST_MS هیچ /t خوانده نشود، به‌جای keepalive فرمان
            ID 19 = 0 هر ESP_LINK_KEEPALIVE_MS تکرار می‌شود تا STM32 خاموشی مود دستی را گزارش کند
            (فریم گم‌شده دوباره فرستاده می‌شود). تب پس‌زمینه با throttle هنوز حدود هر ثانیه می‌خواند. */
    /* [EN] No browser since boot counts as lost: an ESP reset while the STM32 is in manual mode
            must not keep the mode alive for 10 s without anyone watching.
       [FA] نبودن مرورگر از بوت هم «ازدست‌رفته» حساب می‌شود: ری‌استارت ESP وسط مود دستی نباید
            مود را ۱۰ ثانیه بدون ناظر زنده نگه دارد. */
    uint32_t uint32_t__browserAgeMs = uint32_t__nowMs - UINT32_T__G__LastBrowserPollMs;
    bool bool__browserLost = (!BOOL__G__BrowserSeen) || (uint32_t__browserAgeMs >= ESP_LINK_BROWSER_LOST_MS);
    uint32_t uint32_t__keepaliveAgeMs = uint32_t__nowMs - UINT32_T__G__LastKeepaliveMs;
    bool bool__keepaliveDue = bool__manualActive && (uint32_t__keepaliveAgeMs >= ESP_LINK_KEEPALIVE_MS);

    if (bool__keepaliveDue && bool__browserLost)
    {
        func__Esp_SendSetParam(ESP_PARAM_MANUAL_TEST_MODE, 0u);
        return;
    }

    /* [EN] Periodic parameter refresh: the seq-restart detector is blind while the
            sequence counter sits in its wrap window (0xFF00..0xFFFF), so a board reboot
            inside that ~26 s window (once per ~109 min) would leave the applied-value
            table stale forever. One GET_PARAMS every ESP_LINK_PARAM_REFRESH_MS keeps the
            display truthful; it does NOT re-apply user parameters (that only happens on
            a detected restart, so a JIT-parked channel can never be re-armed by this).
       [FA] نوسازی دوره‌ای پارامترها: تشخیص‌گر ری‌استارت در پنجرهٔ wrap شمارندهٔ seq
            (0xFF00..0xFFFF) کور است؛ ریبوت برد در همین پنجرهٔ ~۲۶ ثانیه‌ای (یک‌بار در
            هر ~۱۰۹ دقیقه) جدول مقادیر اعمال‌شده را برای همیشه کهنه می‌گذارد. یک
            GET_PARAMS هر ESP_LINK_PARAM_REFRESH_MS نمایش را راست‌نگه می‌دارد؛
            پارامترهای کاربر را دوباره اعمال نمی‌کند (فقط بعد از ری‌استارتِ
            تشخیص‌شده؛ پس هیچ‌وقت کانال پارک‌شدهٔ JIT را مسلح نمی‌کند). */
    /* [EN] v1.54 (user order 2026-10-05: "the panel no longer displays the
            board's LED/buzzer state, so stop asking the board for things
            nobody needs"): the refresh exists only to keep a WATCHED page
            truthful, so it is now skipped while no browser is polling. With
            the tab closed the STM32 is never asked to dump its 123-value
            parameter table again; the first poll after a gap re-arms it, so
            whoever opens the page still sees fresh applied values.
       [FA] نوسازی دوره‌ای فقط برای راست‌نگه‌داشتن صفحه‌ای است که کسی تماشا
            می‌کند؛ پس تا وقتی مرورگری poll نمی‌کند انجام نمی‌شود و برد دیگر
            مجبور نیست جدول ۱۲۳ مقداری را بی‌خود بفرستد. اولین poll بعد از
            وقفه دوباره مسلحش می‌کند، پس هرکس صفحه را باز کند مقدار تازه
            می‌بیند. */
    if (bool__browserLost)
    {
        UINT32_T__G__LastParamRefreshMs = uint32_t__nowMs - ESP_LINK_PARAM_REFRESH_MS;
    }
    else if ((uint32_t__nowMs - UINT32_T__G__LastParamRefreshMs) >= ESP_LINK_PARAM_REFRESH_MS)
    {
        UINT32_T__G__LastParamRefreshMs = uint32_t__nowMs;
        BOOL__G__TxGetPending = true;
    }
    else
    {
        /* [EN] MISRA 15.7 / [FA] شاخهٔ پایانی */
    }

    if (bool__keepaliveDue)
    {
        BOOL__G__TxGetPending = true;
    }

    if (BOOL__G__TxGetPending)
    {
        BOOL__G__TxGetPending = false;
        func__Esp_WriteFrame(ESP_MSG_GET_PARAMS, NULL, 0u);
    }
}

/* ==================== Frame Receive ==================== */

/**
 * @brief  [EN] Store one parameter value reported by the STM32 (applied value).
 *         [FA] ذخیره مقدار اعمال‌شده یک پارامتر که STM32 گزارش داده است.
 * @param  uint8_t__ptr_item [EN] 5-byte item [id][value LE] / [FA] آیتم ۵ بایتی [id][value LE]
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_StoreParamItem(const uint8_t *uint8_t__ptr_item)
{
    uint8_t uint8_t__id = uint8_t__ptr_item[0];
    if (uint8_t__id < ESP_PARAM_COUNT)
    {
        UINT32_T__G__ParamApplied[uint8_t__id] = func__Esp_ReadU32(uint8_t__ptr_item, 1u);
        BOOL__G__ParamKnown[uint8_t__id] = true;
    }
}

/**
 * @brief  [EN] Add the just-stored TLM frame to the bench statistics window.
 *         [FA] افزودن فریم TLM تازه ذخیره‌شده به پنجرهٔ آمار بنچ.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_StatAccumulate(void)
{
    uint8_t uint8_t__index;

    if (UINT32_T__G__StatCount >= ESP_STAT_MAX_FRAMES)
    {
        return;
    }

    for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        uint32_t uint32_t__value = UINT32_T__G__TlmField[uint8_t__index];
        UINT32_T__G__StatLast[uint8_t__index] = uint32_t__value;
        bool bool__first = (UINT32_T__G__StatCount == 0u);
        UINT32_T__G__StatSum[uint8_t__index] += uint32_t__value;
        if (bool__first || (uint32_t__value < UINT32_T__G__StatMin[uint8_t__index]))
        {
            UINT32_T__G__StatMin[uint8_t__index] = uint32_t__value;
        }
        if (bool__first || (uint32_t__value > UINT32_T__G__StatMax[uint8_t__index]))
        {
            UINT32_T__G__StatMax[uint8_t__index] = uint32_t__value;
        }
    }

    UINT32_T__G__StatFaultOr |= UINT32_T__G__TlmField[ESP_STAT_FAULT_FIELD];
    UINT16_T__G__StatSeq = UINT16_T__G__TlmSeq;
    UINT8_T__G__StatFlags = UINT8_T__G__TlmFlags;
    UINT32_T__G__StatCount++;
}

/**
 * @brief  [EN] Dispatch one checksum-valid frame from the STM32.
 *         [FA] پردازش یک فریم معتبر (checksum درست) دریافتی از STM32.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HandleFrame(void)
{
    const uint8_t *uint8_t__ptr_payload = UINT8_T__G__RxPayload;
    uint8_t uint8_t__index;

    if ((UINT8_T__G__RxType == ESP_MSG_TLM_LIVE) && (UINT16_T__G__RxLen == ESP_LINK_TLM_SIZE))
    {
        uint16_t uint16_t__seqLow = (uint16_t)uint8_t__ptr_payload[0];
        uint16_t uint16_t__seqHigh = (uint16_t)((uint16_t)uint8_t__ptr_payload[1] << 8);
        uint16_t uint16_t__seq = (uint16_t)(uint16_t__seqLow | uint16_t__seqHigh);

        /* [EN] First frame or STM32 restart (seq jumps back): refresh the parameter table.
           [FA] اولین فریم یا ری‌استارت STM32 (عقب‌گرد seq): جدول پارامترها دوباره خوانده شود. */
        bool bool__seqRestart = BOOL__G__TlmSeen && (uint16_t__seq < UINT16_T__G__TlmSeq) && (UINT16_T__G__TlmSeq < 0xFF00u);
        if ((!BOOL__G__TlmSeen) || bool__seqRestart)
        {
            BOOL__G__TxGetPending = true;
        }

        /* [EN] STM32 flash-persists ids 0..14 + 20..75 (v1.14 NVM), so the
                re-send below only matters for the transient test modes
                15..19 (+76): re-send values the user set in this session.
                Manual test mode (ID 19) is never re-enabled automatically.
           [FA] STM32 شناسه‌های ۰..۱۴، ۲۰..۷۵ و ۷۷..۸۲ را روی فلش نگه می‌دارد (NVM
                نسخهٔ ۱.۱۴)، پس ارسال مجدد زیر فقط برای مودهای گذرای تست
                ۱۵..۱۹ (+۷۶) لازم است: مقادیری که کاربر در این نشست داده
                دوباره ارسال شوند. مود تست دستی (شناسه ۱۹) هیچ‌وقت خودکار
                روشن نمی‌شود. */
        if (bool__seqRestart)
        {
            for (uint8_t__index = 0u; uint8_t__index < ESP_PARAM_COUNT; uint8_t__index++)
            {
                bool bool__isManualSwitch = (uint8_t__index == ESP_PARAM_MANUAL_TEST_MODE);
                if (BOOL__G__ParamUserSet[uint8_t__index] && (!bool__isManualSwitch))
                {
                    BOOL__G__TxParamPending[uint8_t__index] = true;
                }
            }
        }

        for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
        {
            uint8_t uint8_t__fieldBytes = (uint8_t)(uint8_t__index * 4u);
            uint8_t uint8_t__offset = (uint8_t)(ESP_LINK_TLM_FIELD_OFFSET + uint8_t__fieldBytes);
            UINT32_T__G__TlmField[uint8_t__index] = func__Esp_ReadU32(uint8_t__ptr_payload, uint8_t__offset);
        }

        UINT16_T__G__TlmSeq = uint16_t__seq;
        UINT8_T__G__TlmFlags = uint8_t__ptr_payload[2];
        /* [EN] v1.43: byte 3 = imbalance scenario 6 status bits.
           [FA] بایت ۳: بیت‌های وضعیت سناریوی ۶ عدم‌توازن. */
        UINT8_T__G__TlmFlags2 = uint8_t__ptr_payload[3];
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        UINT32_T__G__TlmFrameCount++;
        BOOL__G__TlmSeen = true;
        func__Esp_StatAccumulate();
    }
    else if ((UINT8_T__G__RxType == ESP_MSG_PARAM_REPORT) && (UINT16_T__G__RxLen == ESP_LINK_PARAM_ITEM_SIZE))
    {
        func__Esp_StoreParamItem(uint8_t__ptr_payload);
    }
    else if ((UINT8_T__G__RxType == ESP_MSG_PARAMS_BULK) && (UINT16_T__G__RxLen >= 1u))
    {
        /* [EN] v1.16: 77 items need u16 offsets (52 x 5 already overflows u8);
           v1.17: 83 items, same count-driven loop.
           [FA] نسخه ۱.۱۶: ۷۷ آیتم آفست u16 می‌خواهد؛ نسخه ۱.۱۷: ۸۳ آیتم با همان حلقه. */
        uint8_t uint8_t__count = uint8_t__ptr_payload[0];
        uint16_t uint16_t__item;
        for (uint16_t__item = 0u; uint16_t__item < (uint16_t)uint8_t__count; uint16_t__item++)
        {
            uint16_t uint16_t__offset = (uint16_t)(1u + (uint16_t__item * ESP_LINK_PARAM_ITEM_SIZE));
            uint16_t uint16_t__itemEnd = (uint16_t)(uint16_t__offset + ESP_LINK_PARAM_ITEM_SIZE);
            if (uint16_t__itemEnd > UINT16_T__G__RxLen)
            {
                break;
            }
            func__Esp_StoreParamItem(&uint8_t__ptr_payload[uint16_t__offset]);
        }
    }
    else
    {
        /* [EN] Unknown type or wrong length: drop / [FA] نوع ناشناخته یا طول نادرست: دور ریخته می‌شود */
    }
}

/**
 * @brief  [EN] Feed one received byte to the frame parser; resyncs on AA 55.
 *         [FA] دادن یک بایت دریافتی به پارسر فریم؛ با AA 55 همگام‌سازی مجدد می‌کند.
 * @param  uint8_t__byte [EN] Received byte, 0..255 / [FA] بایت دریافتی، ۰ تا ۲۵۵
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_ParseByte(uint8_t uint8_t__byte)
{
    switch (ESP_RX_STATE_T__G__RxState)
    {
        case ESP_RX_WAIT_SOF0:
            if (uint8_t__byte == ESP_LINK_SOF_BYTE0)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF1;
            }
            break;

        case ESP_RX_WAIT_SOF1:
            if (uint8_t__byte == ESP_LINK_SOF_BYTE1)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_VERSION;
            }
            else if (uint8_t__byte != ESP_LINK_SOF_BYTE0)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            }
            else
            {
                /* [EN] AA AA: stay waiting for 55 / [FA] AA AA: منتظر 55 بمان */
            }
            break;

        case ESP_RX_WAIT_VERSION:
            /* [EN] A frame from a firmware we do not speak. Counted and dropped,
               so the page can say "the two boards were flashed out of step"
               instead of showing an empty panel with no explanation.
               [FA] فریمی از فرم‌وری که نمی‌شناسیم. شمرده و رها می‌شود تا صفحه
               بتواند بگوید «دو برد ناهماهنگ فلش شده‌اند» به‌جای نمایش پنل خالی. */
            if (uint8_t__byte != (uint8_t)ESP_LINK_PROTOCOL_VERSION)
            {
                UINT32_T__G__RxVersionMismatch++;
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            }
            else
            {
                UINT16_T__G__RxCrc = func__Esp_Crc16((uint16_t)ESP_LINK_CRC16_INIT, uint8_t__byte);
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_TYPE;
            }
            break;

        case ESP_RX_WAIT_TYPE:
            UINT8_T__G__RxType = uint8_t__byte;
            UINT16_T__G__RxCrc = func__Esp_Crc16(UINT16_T__G__RxCrc, uint8_t__byte);
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_LEN_LO;
            break;

        case ESP_RX_WAIT_LEN_LO:
            UINT16_T__G__RxLen = (uint16_t)uint8_t__byte;
            UINT16_T__G__RxCrc = func__Esp_Crc16(UINT16_T__G__RxCrc, uint8_t__byte);
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_LEN_HI;
            break;

        case ESP_RX_WAIT_LEN_HI:
            UINT16_T__G__RxLen = (uint16_t)(UINT16_T__G__RxLen | ((uint16_t)((uint16_t)uint8_t__byte << 8)));
            UINT16_T__G__RxCrc = func__Esp_Crc16(UINT16_T__G__RxCrc, uint8_t__byte);
            if (UINT16_T__G__RxLen > ESP_LINK_MAX_PAYLOAD)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            }
            else
            {
                UINT16_T__G__RxIndex = 0u;
                ESP_RX_STATE_T__G__RxState = (UINT16_T__G__RxLen == 0u) ? ESP_RX_WAIT_CRC_LO : ESP_RX_WAIT_PAYLOAD;
            }
            break;

        case ESP_RX_WAIT_PAYLOAD:
            UINT8_T__G__RxPayload[UINT16_T__G__RxIndex] = uint8_t__byte;
            UINT16_T__G__RxIndex++;
            UINT16_T__G__RxCrc = func__Esp_Crc16(UINT16_T__G__RxCrc, uint8_t__byte);
            if (UINT16_T__G__RxIndex >= UINT16_T__G__RxLen)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_CRC_LO;
            }
            break;

        case ESP_RX_WAIT_CRC_LO:
            UINT8_T__G__RxCrcLow = uint8_t__byte;
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_CRC_HI;
            break;

        case ESP_RX_WAIT_CRC_HI:
            if (((uint16_t)(((uint16_t)uint8_t__byte << 8) | (uint16_t)UINT8_T__G__RxCrcLow)) == UINT16_T__G__RxCrc)
            {
                func__Esp_HandleFrame();
            }
            else
            {
                UINT32_T__G__RxCrcError++;
            }
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            break;

        default:
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            break;
    }
}
