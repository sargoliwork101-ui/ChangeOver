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

/* ==================== ذخیره فریم تله‌متری ==================== */
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
