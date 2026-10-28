/* simple_link.h - پارسر فریم و CRC پنل ساده
   فقط فارسی - فقط خواندنی، بدون ارسال پارامتر */

/* تاریخچهٔ وضعیت همین‌جا می‌آید تا پارسر بتواند بعد از هر فریم آن را صدا بزند؛
   simple_state.h باید پیش از این فایل include شده باشد. */
#include "simple_log.h"

/* ==================== کمک بایت ==================== */
static uint32_t func__Simple_ReadU32(const uint8_t *uint8_t__ptr_buffer, uint8_t uint8_t__offset)
{
    uint32_t uint32_t__byte0 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset];
    uint32_t uint32_t__byte1 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset + 1u] << 8;
    uint32_t uint32_t__byte2 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset + 2u] << 16;
    uint32_t uint32_t__byte3 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset + 3u] << 24;
    uint32_t uint32_t__lowHalf = uint32_t__byte0 | uint32_t__byte1;
    uint32_t uint32_t__highHalf = uint32_t__byte2 | uint32_t__byte3;
    return uint32_t__lowHalf | uint32_t__highHalf;
}

/* ==================== محاسبه CRC16 ==================== */
static uint16_t func__Simple_Crc16(uint16_t uint16_t__crc, uint8_t uint8_t__byte)
{
    uint8_t uint8_t__bit;
    uint16_t__crc = (uint16_t)(uint16_t__crc ^ ((uint16_t)uint8_t__byte << 8));
    for (uint8_t__bit = 0u; uint8_t__bit < 8u; uint8_t__bit++)
    {
        if ((uint16_t__crc & 0x8000u) != 0u)
        {
            uint16_t__crc = (uint16_t)(((uint16_t)(uint16_t__crc << 1)) ^ (uint16_t)SIMPLE_LINK_CRC16_POLY);
        }
        else
        {
            uint16_t__crc = (uint16_t)(uint16_t__crc << 1);
        }
    }
    return uint16_t__crc;
}

/* ==================== ذخیره فریم تله‌متری و دیاگ ==================== */
/* ==================== دست‌دادن دوطرفه (v1.111) ====================
   دستور کاربر ۲۰۲۶-۱۰-۲۵: «باید برد با ESP یک هندشیک داشته باشد که بفهمم با
   هم ارتباط گرفته‌اند و آن ارتباط درست بوده».
   چرا لازم است: تا امروز تنها نشانهٔ ارتباط، آمدن تله‌متری بود. تله‌متری فقط
   می‌گوید برد حرف می‌زند؛ نمی‌گوید ESP هم به برد می‌رسد. با این دست‌دادن، پنل
   سه چیز جدا را نشان می‌دهد: (۱) پاسخ برد می‌آید، (۲) برد چند فریم سالم از ما
   شنیده و چند تا خراب بوده، (۳) دو طرف یک نسخه و یک تعداد پارامتر دارند. */

/* یک HELLO می‌سازد و می‌فرستد: نسخهٔ سیم، نسخهٔ این پنل، نقش، پرچم‌ها، آپتایم. */
static void func__Simple_SendHello(void)
{
    uint8_t UINT8_T__A__Frame[SIMPLE_LINK_HEADER_SIZE + SIMPLE_HELLO_LEN + SIMPLE_LINK_CRC_SIZE];
    uint8_t UINT8_T__A__Payload[SIMPLE_HELLO_LEN];
    uint16_t uint16_t__crc = SIMPLE_LINK_CRC16_INIT;
    uint32_t uint32_t__uptimeSec = (uint32_t)((uint32_t)millis() / 1000u);
    uint8_t uint8_t__flags = 0u;
    uint8_t uint8_t__i;
    uint32_t uint32_t__nowMs = (uint32_t)millis();

    if (BOOL__G__TlmSeen) uint8_t__flags |= 0x01u;                     /* تله‌متری می‌آید */
    if (BOOL__G__BrowserSeen &&
        ((uint32_t__nowMs - UINT32_T__G__LastBrowserPollMs) < SIMPLE_LINK_TIMEOUT_MS))
    {
        uint8_t__flags |= 0x02u;                                        /* کاربر پنل را باز دارد */
    }

    UINT8_T__A__Payload[0] = (uint8_t)SIMPLE_LINK_PROTOCOL_VERSION;
    UINT8_T__A__Payload[1] = (uint8_t)SIMPLE_ESP_FW_VERSION;
    UINT8_T__A__Payload[2] = (uint8_t)SIMPLE_ESP_ROLE_USER;
    UINT8_T__A__Payload[3] = uint8_t__flags;
    UINT8_T__A__Payload[4] = (uint8_t)(uint32_t__uptimeSec & 0xFFu);
    UINT8_T__A__Payload[5] = (uint8_t)((uint32_t__uptimeSec >> 8) & 0xFFu);
    UINT8_T__A__Payload[6] = (uint8_t)((uint32_t__uptimeSec >> 16) & 0xFFu);
    UINT8_T__A__Payload[7] = (uint8_t)((uint32_t__uptimeSec >> 24) & 0xFFu);

    UINT8_T__A__Frame[0] = SIMPLE_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = SIMPLE_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = (uint8_t)SIMPLE_LINK_PROTOCOL_VERSION;
    UINT8_T__A__Frame[3] = (uint8_t)SIMPLE_MSG_HELLO;
    UINT8_T__A__Frame[4] = (uint8_t)SIMPLE_HELLO_LEN;
    UINT8_T__A__Frame[5] = 0u;
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, (uint8_t)SIMPLE_LINK_PROTOCOL_VERSION);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, (uint8_t)SIMPLE_MSG_HELLO);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, (uint8_t)SIMPLE_HELLO_LEN);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_HELLO_LEN; uint8_t__i++)
    {
        UINT8_T__A__Frame[SIMPLE_LINK_HEADER_SIZE + uint8_t__i] = UINT8_T__A__Payload[uint8_t__i];
        uint16_t__crc = func__Simple_Crc16(uint16_t__crc, UINT8_T__A__Payload[uint8_t__i]);
    }
    UINT8_T__A__Frame[SIMPLE_LINK_HEADER_SIZE + SIMPLE_HELLO_LEN] = (uint8_t)(uint16_t__crc & 0xFFu);
    UINT8_T__A__Frame[SIMPLE_LINK_HEADER_SIZE + SIMPLE_HELLO_LEN + 1u] =
        (uint8_t)((uint16_t__crc >> 8) & 0xFFu);

    (void)Serial.write(UINT8_T__A__Frame,
                       (size_t)(SIMPLE_LINK_HEADER_SIZE + SIMPLE_HELLO_LEN + SIMPLE_LINK_CRC_SIZE));
    UINT32_T__G__LastHelloTxMs = uint32_t__nowMs;
}

/* پاسخ برد را می‌خواند: هویت برد + سلامت دریافت خودش + بازپس آپتایم ما. */
static void func__Simple_HandleHelloAck(const uint8_t *uint8_t__ptr_payload)
{
    uint32_t uint32_t__echoedUptime;

    if (UINT16_T__G__RxLen != (uint16_t)SIMPLE_HELLO_ACK_LEN) return;
    uint32_t__echoedUptime = func__Simple_ReadU32(uint8_t__ptr_payload, 20u);

    UINT8_T__G__BoardProtoVersion = uint8_t__ptr_payload[0];
    UINT8_T__G__BoardFwVersion = uint8_t__ptr_payload[1];
    UINT8_T__G__BoardFlags = uint8_t__ptr_payload[2];
    UINT32_T__G__BoardUptimeSec = func__Simple_ReadU32(uint8_t__ptr_payload, 4u);
    UINT32_T__G__BoardRxFrames = func__Simple_ReadU32(uint8_t__ptr_payload, 8u);
    UINT16_T__G__BoardCrcErrors = (uint16_t)(uint8_t__ptr_payload[12] |
                                             (uint16_t)((uint16_t)uint8_t__ptr_payload[13] << 8));
    UINT16_T__G__BoardVerErrors = (uint16_t)(uint8_t__ptr_payload[14] |
                                             (uint16_t)((uint16_t)uint8_t__ptr_payload[15] << 8));
    UINT16_T__G__BoardParamCount = (uint16_t)(uint8_t__ptr_payload[16] |
                                              (uint16_t)((uint16_t)uint8_t__ptr_payload[17] << 8));
    UINT16_T__G__BoardPeriodMs = (uint16_t)(uint8_t__ptr_payload[18] |
                                            (uint16_t)((uint16_t)uint8_t__ptr_payload[19] << 8));
    (void)uint32_t__echoedUptime;   /* فقط برای تطبیق است؛ نمایش لازم نیست */

    UINT32_T__G__LastHelloAckMs = (uint32_t)millis();
    BOOL__G__HelloSeen = true;
    UINT32_T__G__HelloAckCount++;
}

/* هر دوره یک HELLO؛ اگر پاسخ نرسد، ارتباط را قطع‌شده اعلام می‌کند. */
static void func__Simple_HandshakeTick(uint32_t uint32_t__nowMs)
{
    if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastHelloTxMs) >= SIMPLE_HELLO_INTERVAL_MS)
    {
        func__Simple_SendHello();
    }
    if (BOOL__G__HelloSeen &&
        ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastHelloAckMs) > SIMPLE_HELLO_TIMEOUT_MS))
    {
        BOOL__G__HelloSeen = false;
        UINT32_T__G__HelloLossCount++;
    }
}

static void func__Simple_HandleFrame(void)
{
    const uint8_t *uint8_t__ptr_payload = UINT8_T__G__RxPayload;
    uint8_t uint8_t__index;

    if ((UINT8_T__G__RxType == SIMPLE_MSG_TLM_LIVE) && (UINT16_T__G__RxLen == SIMPLE_LINK_TLM_SIZE))
    {
        uint16_t uint16_t__seqLow = (uint16_t)uint8_t__ptr_payload[0];
        uint16_t uint16_t__seqHigh = (uint16_t)((uint16_t)uint8_t__ptr_payload[1] << 8);
        uint16_t uint16_t__seq = (uint16_t)(uint16_t__seqLow | uint16_t__seqHigh);

        for (uint8_t__index = 0u; uint8_t__index < SIMPLE_LINK_TLM_FIELD_COUNT; uint8_t__index++)
        {
            uint8_t uint8_t__fieldBytes = (uint8_t)(uint8_t__index * 4u);
            uint8_t uint8_t__offset = (uint8_t)(SIMPLE_LINK_TLM_FIELD_OFFSET + uint8_t__fieldBytes);
            UINT32_T__G__TlmField[uint8_t__index] = func__Simple_ReadU32(uint8_t__ptr_payload, uint8_t__offset);
        }

        UINT16_T__G__TlmSeq = uint16_t__seq;
        UINT8_T__G__TlmFlags = uint8_t__ptr_payload[2];
        UINT8_T__G__TlmFlags2 = uint8_t__ptr_payload[3];
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        UINT32_T__G__TlmFrameCount++;
        BOOL__G__TlmSeen = true;
        /* هر فریم به تاریخچه داده می‌شود؛ فقط تغییر واقعی ردیف می‌سازد */
        func__SimpleLog_OnFrame();
    }
    else if (UINT8_T__G__RxType == SIMPLE_MSG_HELLO_ACK)
    {
        func__Simple_HandleHelloAck(uint8_t__ptr_payload);
    }
    else if (UINT8_T__G__RxType == SIMPLE_MSG_DIAG_DATA)
    {
        if (UINT16_T__G__RxLen < 1u) return;
        uint8_t uint8_t__chunk = uint8_t__ptr_payload[0];

        if (uint8_t__chunk == 0u && UINT16_T__G__RxLen >= 23u)
        {
            uint32_t uint32_t__prevCh1 = UINT32_T__G__PrevChargeCountCh1;
            uint32_t uint32_t__prevCh2 = UINT32_T__G__PrevChargeCountCh2;
            UINT32_T__G__TotalChargeSec = func__Simple_ReadU32(uint8_t__ptr_payload, 1u);
            UINT32_T__G__ChargeCount = func__Simple_ReadU32(uint8_t__ptr_payload, 5u);
            UINT32_T__G__UptimeSec = func__Simple_ReadU32(uint8_t__ptr_payload, 9u);
            UINT32_T__G__ResetCount = func__Simple_ReadU32(uint8_t__ptr_payload, 13u);
            UINT32_T__G__BoardTimeSec = func__Simple_ReadU32(uint8_t__ptr_payload, 17u);
            if (UINT32_T__G__BoardTimeSec >= SIMPLE_TIME_VALID_MIN)
            {
                uint32_t uint32_t__panelNowSec = func__Simple_NowSec();
                if ((!BOOL__G__TimeValid) ||
                    (UINT32_T__G__BoardTimeSec > uint32_t__panelNowSec) ||
                    ((uint32_t__panelNowSec - UINT32_T__G__BoardTimeSec) >= SIMPLE_TIME_ADOPT_DIFF_SEC))
                {
                    func__Simple_TimeSet(UINT32_T__G__BoardTimeSec, false);
                    BOOL__G__TimePushed = true;
                }
            }
            uint8_t uint8_t__newErr = uint8_t__ptr_payload[21];
            uint8_t uint8_t__newPwr = uint8_t__ptr_payload[22];
            if (uint8_t__newErr > SIMPLE_DIAG_MAX_ERRORS) uint8_t__newErr = SIMPLE_DIAG_MAX_ERRORS;
            if (uint8_t__newPwr > SIMPLE_DIAG_MAX_POWER_EVENTS) uint8_t__newPwr = SIMPLE_DIAG_MAX_POWER_EVENTS;
            if (BOOL__G__DiagClearPending)
            {
                if ((uint8_t__newErr == 0u) && (uint8_t__newPwr == 0u))
                {
                    BOOL__G__DiagClearPending = false;
                }
                else
                {
                    uint8_t__newErr = 0u;
                    uint8_t__newPwr = 0u;
                }
            }
            /* اگر برد اصلی صفر گزارش داد ولی ما محلی خطا داریم، محلی را نگه دار */
            if (uint8_t__newErr == 0u && UINT8_T__G__DiagErrorCount > 0u)
            {
                /* نگه‌داشتن خطاهای محلی */
            }
            else if (uint8_t__newErr > 0u)
            {
                if (uint8_t__newErr > UINT8_T__G__DiagErrorCount)
                {
                    UINT8_T__G__DiagErrorCount = uint8_t__newErr;
                }
                else if (UINT8_T__G__DiagErrorCount == 0u)
                {
                    UINT8_T__G__DiagErrorCount = uint8_t__newErr;
                }
                /* اگر محلی بیشتر بود، تعداد محلی می‌ماند تا پاک نشود */
            }
            else
            {
                UINT8_T__G__DiagErrorCount = uint8_t__newErr;
            }

            if (uint8_t__newPwr == 0u && UINT8_T__G__PowerEventCount > 0u)
            {
                /* نگه‌داشتن رویدادهای محلی برق */
            }
            else if (uint8_t__newPwr > 0u)
            {
                if (uint8_t__newPwr > UINT8_T__G__PowerEventCount)
                {
                    UINT8_T__G__PowerEventCount = uint8_t__newPwr;
                }
                else if (UINT8_T__G__PowerEventCount == 0u)
                {
                    UINT8_T__G__PowerEventCount = uint8_t__newPwr;
                }
            }
            else
            {
                UINT8_T__G__PowerEventCount = uint8_t__newPwr;
            }
            BOOL__G__DiagValid = true;
            /* توسعه‌یافته: تعداد شارژ هر باتری */
            if (UINT16_T__G__RxLen >= 39u)
            {
                UINT32_T__G__ChargeCountCh1 = func__Simple_ReadU32(uint8_t__ptr_payload, 23u);
                UINT32_T__G__ChargeCountCh2 = func__Simple_ReadU32(uint8_t__ptr_payload, 27u);
                UINT32_T__G__ChargeSecCh1 = func__Simple_ReadU32(uint8_t__ptr_payload, 31u);
                UINT32_T__G__ChargeSecCh2 = func__Simple_ReadU32(uint8_t__ptr_payload, 35u);
                if (!BOOL__G__ChargeCountBaselineReady)
                {
                    UINT32_T__G__PrevChargeCountCh1 = UINT32_T__G__ChargeCountCh1;
                    UINT32_T__G__PrevChargeCountCh2 = UINT32_T__G__ChargeCountCh2;
                    BOOL__G__ChargeCountBaselineReady = true;
                    BOOL__G__ChargeHistDirty = true;
                }
                else
                {
                    /* ثبت تاریخچه شارژ: اگر تعداد افزایش یافت، زمان (ساعت پنل) را ذخیره کن - با بافر چرخشی */
                    uint32_t uint32_t__stampSec = func__Simple_NowSec();
                    if (UINT32_T__G__ChargeCountCh1 < uint32_t__prevCh1)
                    {
                        UINT32_T__G__PrevChargeCountCh1 = UINT32_T__G__ChargeCountCh1;
                        BOOL__G__ChargeHistDirty = true;
                    }
                    else if ((UINT32_T__G__ChargeCountCh1 > uint32_t__prevCh1) && (uint32_t__stampSec != 0u))
                    {
                        BOOL__G__ChargeHistDirty = true;
                        uint32_t uint32_t__diff = UINT32_T__G__ChargeCountCh1 - uint32_t__prevCh1;
                        if (uint32_t__diff > SIMPLE_DIAG_MAX_POWER_EVENTS) uint32_t__diff = SIMPLE_DIAG_MAX_POWER_EVENTS;
                        uint32_t uint32_t__k;
                        for (uint32_t__k = 0u; uint32_t__k < uint32_t__diff; uint32_t__k++)
                        {
                            if (UINT8_T__G__ChargeHistoryCount < SIMPLE_DIAG_MAX_POWER_EVENTS)
                            {
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount].timestamp = uint32_t__stampSec;
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount].channel = 1u;
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount].durationSec = 0u;
                                UINT8_T__G__ChargeHistoryCount++;
                            }
                            else
                            {
                                /* بافر پر - قدیمی‌ترین را حذف و شیفت بده */
                                uint8_t uint8_t__s;
                                for (uint8_t__s = 1u; uint8_t__s < SIMPLE_DIAG_MAX_POWER_EVENTS; uint8_t__s++)
                                {
                                    SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__s - 1u] = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__s];
                                }
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].timestamp = uint32_t__stampSec;
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].channel = 1u;
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].durationSec = 0u;
                            }
                        }
                        UINT32_T__G__PrevChargeCountCh1 = UINT32_T__G__ChargeCountCh1;
                    }
                    if (UINT32_T__G__ChargeCountCh2 < uint32_t__prevCh2)
                    {
                        UINT32_T__G__PrevChargeCountCh2 = UINT32_T__G__ChargeCountCh2;
                        BOOL__G__ChargeHistDirty = true;
                    }
                    else if ((UINT32_T__G__ChargeCountCh2 > uint32_t__prevCh2) && (uint32_t__stampSec != 0u))
                    {
                        BOOL__G__ChargeHistDirty = true;
                        uint32_t uint32_t__diff = UINT32_T__G__ChargeCountCh2 - uint32_t__prevCh2;
                        if (uint32_t__diff > SIMPLE_DIAG_MAX_POWER_EVENTS) uint32_t__diff = SIMPLE_DIAG_MAX_POWER_EVENTS;
                        uint32_t uint32_t__k;
                        for (uint32_t__k = 0u; uint32_t__k < uint32_t__diff; uint32_t__k++)
                        {
                            if (UINT8_T__G__ChargeHistoryCount < SIMPLE_DIAG_MAX_POWER_EVENTS)
                            {
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount].timestamp = uint32_t__stampSec;
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount].channel = 2u;
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount].durationSec = 0u;
                                UINT8_T__G__ChargeHistoryCount++;
                            }
                            else
                            {
                                uint8_t uint8_t__s;
                                for (uint8_t__s = 1u; uint8_t__s < SIMPLE_DIAG_MAX_POWER_EVENTS; uint8_t__s++)
                                {
                                    SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__s - 1u] = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__s];
                                }
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].timestamp = uint32_t__stampSec;
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].channel = 2u;
                                SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].durationSec = 0u;
                            }
                        }
                        UINT32_T__G__PrevChargeCountCh2 = UINT32_T__G__ChargeCountCh2;
                    }
                }
            }
        }
        else if ((uint8_t__chunk == 1u || uint8_t__chunk == 2u) && UINT16_T__G__RxLen >= 3u && !BOOL__G__DiagClearPending)
        {
            uint8_t uint8_t__start = uint8_t__ptr_payload[1];
            uint8_t uint8_t__count = uint8_t__ptr_payload[2];
            uint16_t uint16_t__offset = 3u;
            uint8_t uint8_t__i;
            for (uint8_t__i = 0u; uint8_t__i < uint8_t__count; uint8_t__i++)
            {
                uint8_t uint8_t__idx = (uint8_t)(uint8_t__start + uint8_t__i);
                if (uint8_t__idx >= SIMPLE_DIAG_MAX_ERRORS) break;
                if ((uint16_t__offset + 32u) > UINT16_T__G__RxLen) break;
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__idx].timestamp = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)uint16_t__offset);
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__idx].faultMask = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)(uint16_t__offset + 4u));
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__idx].vIn = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)(uint16_t__offset + 8u));
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__idx].vPack = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)(uint16_t__offset + 12u));
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__idx].vLow = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)(uint16_t__offset + 16u));
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__idx].vHigh = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)(uint16_t__offset + 20u));
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__idx].state1 = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)(uint16_t__offset + 24u));
                SIMPLE_DIAG_ERROR_T__G__Errors[uint8_t__idx].state2 = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)(uint16_t__offset + 28u));
                uint16_t__offset += 32u;
            }
        }
        else if ((uint8_t__chunk == 3u || uint8_t__chunk == 4u) && UINT16_T__G__RxLen >= 3u && !BOOL__G__DiagClearPending)
        {
            uint8_t uint8_t__start = uint8_t__ptr_payload[1];
            uint8_t uint8_t__count = uint8_t__ptr_payload[2];
            uint16_t uint16_t__offset = 3u;
            uint8_t uint8_t__i;
            for (uint8_t__i = 0u; uint8_t__i < uint8_t__count; uint8_t__i++)
            {
                uint8_t uint8_t__idx = (uint8_t)(uint8_t__start + uint8_t__i);
                if (uint8_t__idx >= SIMPLE_DIAG_MAX_POWER_EVENTS) break;
                if ((uint16_t__offset + 12u) > UINT16_T__G__RxLen) break;
                SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__idx].offTime = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)uint16_t__offset);
                SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__idx].onTime = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)(uint16_t__offset + 4u));
                SIMPLE_POWER_EVENT_T__G__PowerEvents[uint8_t__idx].durationSec = func__Simple_ReadU32(uint8_t__ptr_payload, (uint8_t)(uint16_t__offset + 8u));
                uint16_t__offset += 12u;
            }
        }
    }
    else if ((UINT8_T__G__RxType == SIMPLE_MSG_PARAM_REPORT) && (UINT16_T__G__RxLen >= SIMPLE_PARAM_ITEM_SIZE))
    {
        /* یک پارامتر که برد تازه اعمال کرده است: [شناسه:u8][مقدار:u32 LE] */
        uint8_t uint8_t__id = uint8_t__ptr_payload[0];
        uint32_t uint32_t__value = func__Simple_ReadU32(uint8_t__ptr_payload, 1u);
        if ((uint8_t__id >= SIMPLE_SLOT_FIRST_ID) && (uint8_t__id <= SIMPLE_SLOT_LAST_ID))
        {
            /* پژواک یک درخواست ریست: برد مقدار را اعمال کرده است. مقدار
               اعمال‌شده ذخیره و خانهٔ همان درخواست از صف آزاد می‌شود. */
            uint8_t uint8_t__i;
            UINT8_T__G__SlotLastId = uint8_t__id;
            UINT32_T__G__SlotLastValue = uint32_t__value;
            BOOL__G__SlotLastKnown = true;
            UINT32_T__G__SlotEchoCount++;
            for (uint8_t__i = 0u; uint8_t__i < SIMPLE_SLOT_QUEUE_LEN; uint8_t__i++)
            {
                if (BOOL__G__SlotQueuePending[uint8_t__i] &&
                    (UINT8_T__G__SlotQueueId[uint8_t__i] == uint8_t__id))
                {
                    BOOL__G__SlotQueuePending[uint8_t__i] = false;
                }
            }
        }
        else
        {
            func__Simple_StoreBoardParam(uint8_t__id, uint32_t__value);
        }
    }
    else if ((UINT8_T__G__RxType == SIMPLE_MSG_PARAM_NAK) && (UINT16_T__G__RxLen == 2u))
    {
        /* برد یک نوشتن را نپذیرفت: [شناسه:u8][کد:u8] */
        if ((uint8_t__ptr_payload[0] >= SIMPLE_SLOT_FIRST_ID) &&
            (uint8_t__ptr_payload[0] <= SIMPLE_SLOT_LAST_ID))
        {
            UINT8_T__G__SlotNakId = uint8_t__ptr_payload[0];
            UINT8_T__G__SlotNakCode = uint8_t__ptr_payload[1];
            UINT32_T__G__SlotNakCount++;
        }
    }
    else if ((UINT8_T__G__RxType == SIMPLE_MSG_PARAMS_BULK) && (UINT16_T__G__RxLen >= 1u))
    {
        /* پارامترهای خود برد: [تعداد:u8] بعد هر آیتم [شناسه:u8][مقدار:u32 LE].
           قاب بزرگ است (۱۰۲ آیتم = ۵۱۱ بایت) پس آفست باید ۱۶ بیتی باشد.
           کلید ذخیره شناسه است، نه ترتیب - پس چند فریم پشت سر هم مشکلی ندارند. */
        uint8_t uint8_t__count = uint8_t__ptr_payload[0];
        uint16_t uint16_t__item;
        for (uint16_t__item = 0u; uint16_t__item < (uint16_t)uint8_t__count; uint16_t__item++)
        {
            uint16_t uint16_t__offset = (uint16_t)(1u + (uint16_t__item * SIMPLE_PARAM_ITEM_SIZE));
            uint16_t uint16_t__itemEnd = (uint16_t)(uint16_t__offset + SIMPLE_PARAM_ITEM_SIZE);
            uint32_t uint32_t__byte0;
            uint32_t uint32_t__byte1;
            uint32_t uint32_t__byte2;
            uint32_t uint32_t__byte3;
            uint32_t uint32_t__value;
            if (uint16_t__itemEnd > UINT16_T__G__RxLen) break;
            uint32_t__byte0 = (uint32_t)uint8_t__ptr_payload[uint16_t__offset + 1u];
            uint32_t__byte1 = (uint32_t)uint8_t__ptr_payload[uint16_t__offset + 2u] << 8;
            uint32_t__byte2 = (uint32_t)uint8_t__ptr_payload[uint16_t__offset + 3u] << 16;
            uint32_t__byte3 = (uint32_t)uint8_t__ptr_payload[uint16_t__offset + 4u] << 24;
            uint32_t__value = uint32_t__byte0 | uint32_t__byte1;
            uint32_t__value |= (uint32_t__byte2 | uint32_t__byte3);
            func__Simple_StoreBoardParam(uint8_t__ptr_payload[uint16_t__offset], uint32_t__value);
        }
        UINT32_T__G__ParamBulkFrames++;
    }
}

/* ==================== ریست دستی قفل‌ها (فرستنده و صف) ==================== */
/**
 * @brief  یک درخواست ریست را در صف اسلات‌ها می‌گذارد.
 * @param  uint8_t__id شناسهٔ اسلات (۲۰۰..۲۰۳)؛ بیرون این بازه رد می‌شود
 * @param  uint32_t__value مقدار اعمالی؛ برای ریست صفر
 * @return true اگر در صف نشست (یا ادغام شد)، false اگر صف پر بود
 */
static bool func__Simple_QueueSlot(uint8_t uint8_t__id, uint32_t uint32_t__value)
{
    uint8_t uint8_t__i;

    if ((uint8_t__id < SIMPLE_SLOT_FIRST_ID) || (uint8_t__id > SIMPLE_SLOT_LAST_ID))
    {
        return false;
    }

    /* درخواست تکراری برای همان شناسه دوباره صف نمی‌شود؛ مقدارش تازه می‌شود
       تا دو کلیک پشت‌سرهم دو فریم بی‌فایده نسازد. */
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_SLOT_QUEUE_LEN; uint8_t__i++)
    {
        if (BOOL__G__SlotQueuePending[uint8_t__i] && (UINT8_T__G__SlotQueueId[uint8_t__i] == uint8_t__id))
        {
            UINT32_T__G__SlotQueueValue[uint8_t__i] = uint32_t__value;
            return true;
        }
    }

    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_SLOT_QUEUE_LEN; uint8_t__i++)
    {
        if (BOOL__G__SlotQueuePending[uint8_t__i] == false)
        {
            UINT8_T__G__SlotQueueId[uint8_t__i] = uint8_t__id;
            UINT32_T__G__SlotQueueValue[uint8_t__i] = uint32_t__value;
            BOOL__G__SlotQueuePending[uint8_t__i] = true;
            UINT32_T__G__SlotRequestCount++;
            return true;
        }
    }

    UINT8_T__G__SlotBusyReject++;
    return false;
}

/**
 * @brief  یک فریم SET_PARAM برای برد می‌فرستد (شناسه + مقدار ۳۲بیتی).
 * @param  uint8_t__id شناسهٔ اسلات، uint32_t__value مقدار
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Simple_SendSetParam(uint8_t uint8_t__id, uint32_t uint32_t__value)
{
    uint8_t UINT8_T__A__Frame[13];
    uint8_t UINT8_T__A__Payload[5];
    uint16_t uint16_t__crc = SIMPLE_LINK_CRC16_INIT;
    uint8_t uint8_t__i;

    UINT8_T__A__Payload[0] = uint8_t__id;
    UINT8_T__A__Payload[1] = (uint8_t)(uint32_t__value & 0xFFu);
    UINT8_T__A__Payload[2] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
    UINT8_T__A__Payload[3] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
    UINT8_T__A__Payload[4] = (uint8_t)((uint32_t__value >> 24) & 0xFFu);

    UINT8_T__A__Frame[0] = SIMPLE_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = SIMPLE_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = SIMPLE_LINK_PROTOCOL_VERSION;
    UINT8_T__A__Frame[3] = SIMPLE_MSG_SET_PARAM;
    UINT8_T__A__Frame[4] = 5u;
    UINT8_T__A__Frame[5] = 0u;
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_LINK_PROTOCOL_VERSION);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_MSG_SET_PARAM);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 5u);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    for (uint8_t__i = 0u; uint8_t__i < 5u; uint8_t__i++)
    {
        UINT8_T__A__Frame[6u + uint8_t__i] = UINT8_T__A__Payload[uint8_t__i];
        uint16_t__crc = func__Simple_Crc16(uint16_t__crc, UINT8_T__A__Payload[uint8_t__i]);
    }
    UINT8_T__A__Frame[11] = (uint8_t)(uint16_t__crc & 0xFFu);
    UINT8_T__A__Frame[12] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
    (void)Serial.write(UINT8_T__A__Frame, 13u);
}

/**
 * @brief  صف ریست را خالی می‌کند: هر SIMPLE_SLOT_TX_GAP_MS یک فریم.
 *         تأیید نشدن هم مهم نیست؛ پنل از پژواک REPORT می‌فهمد برد مقدار را
 *         اعمال کرده یا نه (و NAK هم جدا شمرده می‌شود).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__Simple_SlotTick(void)
{
    uint32_t uint32_t__now = (uint32_t)millis();
    uint8_t uint8_t__i;

    if ((uint32_t__now - UINT32_T__G__LastSlotTxMs) < SIMPLE_SLOT_TX_GAP_MS)
    {
        return;
    }

    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_SLOT_QUEUE_LEN; uint8_t__i++)
    {
        if (BOOL__G__SlotQueuePending[uint8_t__i])
        {
            BOOL__G__SlotQueuePending[uint8_t__i] = false;
            func__Simple_SendSetParam(UINT8_T__G__SlotQueueId[uint8_t__i],
                                      UINT32_T__G__SlotQueueValue[uint8_t__i]);
            UINT32_T__G__LastSlotTxMs = uint32_t__now;
            return;
        }
    }
}

/* ==================== همگام‌سازی مجدد ==================== */
static void func__Simple_ResyncFromByte(uint8_t uint8_t__byte)
{
    SIMPLE_RX_STATE_T__G__RxState =
        (uint8_t__byte == SIMPLE_LINK_SOF_BYTE0) ? SIMPLE_RX_WAIT_SOF1 : SIMPLE_RX_WAIT_SOF0;
}

/* ==================== پارسر یک بایت ==================== */
static void func__Simple_ParseByte(uint8_t uint8_t__byte)
{
    switch (SIMPLE_RX_STATE_T__G__RxState)
    {
        case SIMPLE_RX_WAIT_SOF0:
            if (uint8_t__byte == SIMPLE_LINK_SOF_BYTE0)
            {
                SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_SOF1;
            }
            break;

        case SIMPLE_RX_WAIT_SOF1:
            if (uint8_t__byte == SIMPLE_LINK_SOF_BYTE1)
            {
                SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_VERSION;
            }
            else if (uint8_t__byte != SIMPLE_LINK_SOF_BYTE0)
            {
                SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_SOF0;
            }
            break;

        case SIMPLE_RX_WAIT_VERSION:
            if (uint8_t__byte != (uint8_t)SIMPLE_LINK_PROTOCOL_VERSION)
            {
                UINT32_T__G__RxVersionMismatch++;
                func__Simple_ResyncFromByte(uint8_t__byte);
            }
            else
            {
                UINT16_T__G__RxCrc = func__Simple_Crc16((uint16_t)SIMPLE_LINK_CRC16_INIT, uint8_t__byte);
                SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_TYPE;
            }
            break;

        case SIMPLE_RX_WAIT_TYPE:
            UINT8_T__G__RxType = uint8_t__byte;
            UINT16_T__G__RxCrc = func__Simple_Crc16(UINT16_T__G__RxCrc, uint8_t__byte);
            SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_LEN_LO;
            break;

        case SIMPLE_RX_WAIT_LEN_LO:
            UINT16_T__G__RxLen = (uint16_t)uint8_t__byte;
            UINT16_T__G__RxCrc = func__Simple_Crc16(UINT16_T__G__RxCrc, uint8_t__byte);
            SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_LEN_HI;
            break;

        case SIMPLE_RX_WAIT_LEN_HI:
            UINT16_T__G__RxLen = (uint16_t)(UINT16_T__G__RxLen | ((uint16_t)((uint16_t)uint8_t__byte << 8)));
            UINT16_T__G__RxCrc = func__Simple_Crc16(UINT16_T__G__RxCrc, uint8_t__byte);
            if (UINT16_T__G__RxLen > SIMPLE_LINK_MAX_PAYLOAD)
            {
                func__Simple_ResyncFromByte(uint8_t__byte);
            }
            else
            {
                UINT16_T__G__RxIndex = 0u;
                SIMPLE_RX_STATE_T__G__RxState = (UINT16_T__G__RxLen == 0u) ? SIMPLE_RX_WAIT_CRC_LO : SIMPLE_RX_WAIT_PAYLOAD;
            }
            break;

        case SIMPLE_RX_WAIT_PAYLOAD:
            UINT8_T__G__RxPayload[UINT16_T__G__RxIndex] = uint8_t__byte;
            UINT16_T__G__RxIndex++;
            UINT16_T__G__RxCrc = func__Simple_Crc16(UINT16_T__G__RxCrc, uint8_t__byte);
            if (UINT16_T__G__RxIndex >= UINT16_T__G__RxLen)
            {
                SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_CRC_LO;
            }
            break;

        case SIMPLE_RX_WAIT_CRC_LO:
            UINT8_T__G__RxCrcLow = uint8_t__byte;
            SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_CRC_HI;
            break;

        case SIMPLE_RX_WAIT_CRC_HI:
            if (((uint16_t)(((uint16_t)uint8_t__byte << 8) | (uint16_t)UINT8_T__G__RxCrcLow)) == UINT16_T__G__RxCrc)
            {
                func__Simple_HandleFrame();
            }
            else
            {
                UINT32_T__G__RxCrcError++;
            }
            func__Simple_ResyncFromByte(uint8_t__byte);
            break;

        default:
            func__Simple_ResyncFromByte(uint8_t__byte);
            break;
    }
}

/* ==================== تخلیه UART ==================== */
static void func__Simple_DrainSerial(void)
{
    while (Serial.available() > 0)
    {
        int32_t int32_t__byte = (int32_t)Serial.read();
        if (int32_t__byte >= 0)
        {
            func__Simple_ParseByte((uint8_t)int32_t__byte);
        }
    }
}

/* ==================== ارسال وضعیت اتصال کاربر وای‌فای به برد ==================== */
static void func__Simple_SendWifiClientState(bool bool__connected)
{
    uint8_t UINT8_T__A__Frame[9];
    uint8_t uint8_t__stateByte = bool__connected ? 1u : 0u;
    uint16_t uint16_t__crc = SIMPLE_LINK_CRC16_INIT;

    UINT8_T__A__Frame[0] = SIMPLE_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = SIMPLE_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = SIMPLE_LINK_PROTOCOL_VERSION;
    UINT8_T__A__Frame[3] = SIMPLE_MSG_WIFI_CLIENT_STATE;
    UINT8_T__A__Frame[4] = 1u;
    UINT8_T__A__Frame[5] = 0u;
    UINT8_T__A__Frame[6] = uint8_t__stateByte;
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_LINK_PROTOCOL_VERSION);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, SIMPLE_MSG_WIFI_CLIENT_STATE);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 1u);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, uint8_t__stateByte);
    UINT8_T__A__Frame[7] = (uint8_t)(uint16_t__crc & 0xFFu);
    UINT8_T__A__Frame[8] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
    (void)Serial.write(UINT8_T__A__Frame, 9u);
}

/* ==================== بررسی اتصال فعال کاربر به وای‌فای یا پنل ==================== */
static bool func__Simple_IsWifiClientConnected(uint32_t uint32_t__nowMs)
{
    bool bool__stationConnected = (WiFi.softAPgetStationNum() > 0);
    bool bool__browserActive = BOOL__G__BrowserSeen &&
        ((uint32_t__nowMs - UINT32_T__G__LastBrowserPollMs) < SIMPLE_WIFI_CLIENT_HOLD_MS);
    return bool__stationConnected || bool__browserActive;
}

/* ==================== اعمال توان آنتن ====================
   یک جا، با یک پرچم آینه‌ای که همیشه وضعیت واقعاً اعمال‌شده را می‌گوید.
   چرا تابع جدا: هنگام ساخت/بازسازی اکسس‌پوینت باید توان دوباره نوشته شود
   (مقدار قبلی روی چیپ می‌ماند)؛ با پرچم آینه‌ای بدون this force، نوشتن جا
   می‌افتاد و توان اشتباه می‌ماند. */
static void func__Simple_ApplyTxPower(bool bool__high, bool bool__force)
{
    if ((bool__force == false) && (BOOL__G__WifiHighPower == bool__high))
    {
        return;
    }
#if defined(ESP8266)
    WiFi.setOutputPower(bool__high ? SIMPLE_WIFI_TX_MAX_DBM : SIMPLE_WIFI_TX_LOW_DBM);
#endif
    BOOL__G__WifiHighPower = bool__high;
}

/* ==================== محاسبهٔ درصد شارژ باتری برای حد قطع وای‌فای ==================== */
static uint32_t func__Simple_ComputeBatteryPercent(void)
{
    uint32_t uint32_t__vBat24Mv = UINT32_T__G__TlmField[15];
    uint32_t uint32_t__vMinMv = func__Simple_BoardParam(SIMPLE_PARAM_PCT_VMIN_MV);
    uint32_t uint32_t__vMaxMv = func__Simple_BoardParam(SIMPLE_PARAM_PCT_VMAX_MV);

    if ((!BOOL__G__TlmSeen) || (uint32_t__vBat24Mv == 0u) || (uint32_t__vMaxMv <= uint32_t__vMinMv))
    {
        return 100u;
    }
    if (uint32_t__vBat24Mv <= uint32_t__vMinMv)
    {
        return 0u;
    }
    if (uint32_t__vBat24Mv >= uint32_t__vMaxMv)
    {
        return 100u;
    }
    return ((uint32_t__vBat24Mv - uint32_t__vMinMv) * 100u) / (uint32_t__vMaxMv - uint32_t__vMinMv);
}

/* ==================== مدیریت ترکیبی قدرت آنتن و چرخهٔ خاموش/روشن وای‌فای روی باتری ==================== */
static void func__Simple_WifiPowerSaveTick(uint32_t uint32_t__nowMs)
{
    bool bool__inputPresent = (!BOOL__G__TlmSeen) ||
        ((UINT8_T__G__TlmFlags & SIMPLE_TLM_FLAG_INPUT_PRESENT) != 0u);
    bool bool__stationConnected = (WiFi.softAPgetStationNum() > 0);
    bool bool__clientConnected = func__Simple_IsWifiClientConnected(uint32_t__nowMs);
    uint32_t uint32_t__idleLowPowerMs = func__Simple_BoardParam(SIMPLE_PARAM_WIFI_IDLE_LOW_POWER_MS);
    uint32_t uint32_t__cycleOnMs = func__Simple_BoardParam(SIMPLE_PARAM_WIFI_CYCLE_ON_MS);
    uint32_t uint32_t__cycleOffMs = func__Simple_BoardParam(SIMPLE_PARAM_WIFI_CYCLE_OFF_MS);
    uint32_t uint32_t__batCutoffPct = func__Simple_BoardParam(SIMPLE_PARAM_WIFI_BAT_CUTOFF_PCT);
    uint32_t uint32_t__batteryPct = func__Simple_ComputeBatteryPercent();

    if (bool__inputPresent || (uint32_t__batCutoffPct == 0u))
    {
        BOOL__G__WifiBatCutoffLatched = false;
    }
    else if (uint32_t__batteryPct < uint32_t__batCutoffPct)
    {
        BOOL__G__WifiBatCutoffLatched = true;
    }
    else if (BOOL__G__WifiBatCutoffLatched)
    {
        uint32_t uint32_t__recoverPct = uint32_t__batCutoffPct + 2u;
        if (uint32_t__recoverPct > 100u)
        {
            uint32_t__recoverPct = 100u;
        }
        if (uint32_t__batteryPct >= uint32_t__recoverPct)
        {
            BOOL__G__WifiBatCutoffLatched = false;
        }
    }

    if ((!bool__inputPresent) && BOOL__G__WifiBatCutoffLatched)
    {
        BOOL__G__WifiClientConnected = false;
        if (BOOL__G__WifiLastReportedConnected)
        {
            func__Simple_SendWifiClientState(false);
            BOOL__G__WifiLastReportedConnected = false;
            UINT32_T__G__WifiLastReportMs = uint32_t__nowMs;
        }
        if (BOOL__G__WifiApActive)
        {
            (void)WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_OFF);
            BOOL__G__WifiApActive = false;
        }
        return;
    }

    BOOL__G__WifiClientConnected = bool__clientConnected;

    if ((bool__clientConnected != BOOL__G__WifiLastReportedConnected) ||
        (bool__clientConnected && (!bool__inputPresent) &&
         ((uint32_t__nowMs - UINT32_T__G__WifiLastReportMs) >= SIMPLE_WIFI_CLIENT_REPORT_MS)))
    {
        func__Simple_SendWifiClientState(bool__clientConnected);
        BOOL__G__WifiLastReportedConnected = bool__clientConnected;
        UINT32_T__G__WifiLastReportMs = uint32_t__nowMs;
    }

    /* v1.112: از چه لحظه‌ای یک دستگاه به شبکه وصل شده است (۰ = وصل نیست). */
    if (bool__stationConnected)
    {
        if (UINT32_T__G__WifiStationSinceMs == 0u)
        {
            UINT32_T__G__WifiStationSinceMs = uint32_t__nowMs;
        }
    }
    else
    {
        UINT32_T__G__WifiStationSinceMs = 0u;
    }

    if (bool__inputPresent || bool__clientConnected)
    {
        bool bool__apJustCreated = false;
        bool bool__viewerActive;
        bool bool__newcomer;
        bool bool__wantHighPower;

        BOOL__G__WifiBatteryIdleTracking = false;
        UINT32_T__G__WifiBatteryIdleStartMs = uint32_t__nowMs;
        if (!BOOL__G__WifiApActive)
        {
            WiFi.mode(WIFI_AP);
            (void)WiFi.softAP(CHAR__G__WifiSsid, CHAR__G__WifiPass);
            BOOL__G__WifiApActive = true;
            bool__apJustCreated = true;
        }

        /* دستور کاربر ۲۰۲۶-۱۰-۲۵: توان بیشینه تنها وقتی هزینه‌اش می‌ارزد که
           کسی واقعاً پنل را ببیند. سه حالت:
             - برق شهر: همیشه بیشینه (هزینهٔ باتری ندارد).
             - بیننده: درخواست HTTP تازه در ۱۰ دقیقهٔ اخیر.
             - تازه‌وارد: دستگاهی در ۱۰ دقیقهٔ اخیر به شبکه وصل شده
               (تا فرصت باز کردن پنل داشته باشد).
           غیر از این، اکسس‌پوینت روشن می‌ماند ولی با توان کم؛ پس گوشی‌ای که
           فقط به شبکه وصل مانده دیگر باتری را با پرتوانی نمی‌سوزاند. */
        bool__viewerActive = BOOL__G__BrowserSeen &&
            ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastBrowserPollMs) <
             (uint32_t)SIMPLE_WIFI_VIEWER_HOLD_MS);
        bool__newcomer = bool__stationConnected &&
            (UINT32_T__G__WifiStationSinceMs != 0u) &&
            ((uint32_t)(uint32_t__nowMs - UINT32_T__G__WifiStationSinceMs) <
             (uint32_t)SIMPLE_WIFI_VIEWER_HOLD_MS);
        bool__wantHighPower = bool__inputPresent || bool__viewerActive || bool__newcomer;

        func__Simple_ApplyTxPower(bool__wantHighPower, bool__apJustCreated);
        return;
    }

    if (!BOOL__G__WifiBatteryIdleTracking)
    {
        BOOL__G__WifiBatteryIdleTracking = true;
        UINT32_T__G__WifiBatteryIdleStartMs = uint32_t__nowMs;
    }

    uint32_t uint32_t__idleElapsedMs = uint32_t__nowMs - UINT32_T__G__WifiBatteryIdleStartMs;
    bool bool__shouldApBeOn = true;

    if ((uint32_t__idleElapsedMs >= uint32_t__idleLowPowerMs) &&
        (uint32_t__cycleOffMs > 0u))
    {
        uint32_t uint32_t__afterIdleMs =
            uint32_t__idleElapsedMs - uint32_t__idleLowPowerMs;
        uint32_t uint32_t__cyclePeriodMs = uint32_t__cycleOffMs + uint32_t__cycleOnMs;
        uint32_t uint32_t__cycleOffsetMs =
            uint32_t__afterIdleMs % uint32_t__cyclePeriodMs;
        bool__shouldApBeOn = (uint32_t__cycleOffsetMs >= uint32_t__cycleOffMs);
    }

    if (bool__shouldApBeOn)
    {
        bool bool__apJustCreated = false;

        if (!BOOL__G__WifiApActive)
        {
            WiFi.mode(WIFI_AP);
            (void)WiFi.softAP(CHAR__G__WifiSsid, CHAR__G__WifiPass);
            BOOL__G__WifiApActive = true;
            bool__apJustCreated = true;
        }
        /* در چرخهٔ باتری هیچ بیننده‌ای نیست: همیشه توان کم. */
        func__Simple_ApplyTxPower(false, bool__apJustCreated);
    }
    else
    {
        if (BOOL__G__WifiApActive)
        {
            (void)WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_OFF);
            BOOL__G__WifiApActive = false;
        }
    }
}
