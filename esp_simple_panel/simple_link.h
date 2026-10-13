/* simple_link.h - پارسر فریم و CRC پنل ساده
   فقط فارسی - فقط خواندنی، بدون ارسال پارامتر */

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
    }
    else if (UINT8_T__G__RxType == SIMPLE_MSG_DIAG_DATA)
    {
        if (UINT16_T__G__RxLen < 1u) return;
        uint8_t uint8_t__chunk = uint8_t__ptr_payload[0];

        if (uint8_t__chunk == 0u && UINT16_T__G__RxLen >= 23u)
        {
            uint32_t uint32_t__prevCh1 = UINT32_T__G__ChargeCountCh1;
            uint32_t uint32_t__prevCh2 = UINT32_T__G__ChargeCountCh2;
            UINT32_T__G__TotalChargeSec = func__Simple_ReadU32(uint8_t__ptr_payload, 1u);
            UINT32_T__G__ChargeCount = func__Simple_ReadU32(uint8_t__ptr_payload, 5u);
            UINT32_T__G__UptimeSec = func__Simple_ReadU32(uint8_t__ptr_payload, 9u);
            UINT32_T__G__ResetCount = func__Simple_ReadU32(uint8_t__ptr_payload, 13u);
            UINT32_T__G__BoardTimeSec = func__Simple_ReadU32(uint8_t__ptr_payload, 17u);
            uint8_t uint8_t__newErr = uint8_t__ptr_payload[21];
            uint8_t uint8_t__newPwr = uint8_t__ptr_payload[22];
            if (uint8_t__newErr > SIMPLE_DIAG_MAX_ERRORS) uint8_t__newErr = SIMPLE_DIAG_MAX_ERRORS;
            if (uint8_t__newPwr > SIMPLE_DIAG_MAX_POWER_EVENTS) uint8_t__newPwr = SIMPLE_DIAG_MAX_POWER_EVENTS;
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
                /* ثبت تاریخچه شارژ: اگر تعداد افزایش یافت، زمان برد را ذخیره کن - با بافر چرخشی */
                if ((UINT32_T__G__ChargeCountCh1 > uint32_t__prevCh1) && (UINT32_T__G__BoardTimeSec != 0u))
                {
                    uint32_t uint32_t__diff = UINT32_T__G__ChargeCountCh1 - uint32_t__prevCh1;
                    if (uint32_t__diff > SIMPLE_DIAG_MAX_POWER_EVENTS) uint32_t__diff = SIMPLE_DIAG_MAX_POWER_EVENTS;
                    uint32_t uint32_t__k;
                    for (uint32_t__k = 0u; uint32_t__k < uint32_t__diff; uint32_t__k++)
                    {
                        if (UINT8_T__G__ChargeHistoryCount < SIMPLE_DIAG_MAX_POWER_EVENTS)
                        {
                            SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount].timestamp = UINT32_T__G__BoardTimeSec;
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
                            SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].timestamp = UINT32_T__G__BoardTimeSec;
                            SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].channel = 1u;
                            SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].durationSec = 0u;
                        }
                    }
                    UINT32_T__G__PrevChargeCountCh1 = UINT32_T__G__ChargeCountCh1;
                }
                if ((UINT32_T__G__ChargeCountCh2 > uint32_t__prevCh2) && (UINT32_T__G__BoardTimeSec != 0u))
                {
                    uint32_t uint32_t__diff = UINT32_T__G__ChargeCountCh2 - uint32_t__prevCh2;
                    if (uint32_t__diff > SIMPLE_DIAG_MAX_POWER_EVENTS) uint32_t__diff = SIMPLE_DIAG_MAX_POWER_EVENTS;
                    uint32_t uint32_t__k;
                    for (uint32_t__k = 0u; uint32_t__k < uint32_t__diff; uint32_t__k++)
                    {
                        if (UINT8_T__G__ChargeHistoryCount < SIMPLE_DIAG_MAX_POWER_EVENTS)
                        {
                            SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount].timestamp = UINT32_T__G__BoardTimeSec;
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
                            SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].timestamp = UINT32_T__G__BoardTimeSec;
                            SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].channel = 2u;
                            SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS - 1u].durationSec = 0u;
                        }
                    }
                    UINT32_T__G__PrevChargeCountCh2 = UINT32_T__G__ChargeCountCh2;
                }
            }
        }
        else if ((uint8_t__chunk == 1u || uint8_t__chunk == 2u) && UINT16_T__G__RxLen >= 3u)
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
        else if ((uint8_t__chunk == 3u || uint8_t__chunk == 4u) && UINT16_T__G__RxLen >= 3u)
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
