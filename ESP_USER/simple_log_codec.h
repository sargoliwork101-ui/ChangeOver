/*
 * simple_log_codec.h - compact, lossless day-file codec for the user panel log.
 *
 * Ported from ESP_Calibration/plink_log_codec.h (v1.138) on the owner's order
 * 2026-11-04: "I wrote a better logging method there, bring it to your log".
 * Same on-disk format, same guarantees, 22 fields instead of 18.
 *
 * - Every record is appended immediately with its own length, sync word and
 *   CRC16; nothing is batched, so a power cut loses at most the row in flight.
 * - At least every 16th record is a full row (checkpoint), so a damaged delta
 *   only costs the rows until the next full one - the rest of the day still
 *   decodes and the CSV export stays exact.
 * - Damaged bytes are skipped by sliding one byte forward; a delta is never
 *   decoded without a valid base row.
 * - The CSV the panel and the download see is rebuilt from these records, so
 *   the public text form did not change.
 */
#ifndef SIMPLE_LOG_CODEC_H
#define SIMPLE_LOG_CODEC_H
struct SimpleLogCodec
{
    /* 22 CSV fields, 8 header bytes, at most 22*5 payload bytes, checkpoint 16 */
    enum
    {
        Fields = 22,
        HeaderSize = 8,
        MaxFrame = 6 + Fields * 5,
        Checkpoint = 16
    };
    struct State
    {
        uint32_t value[Fields];
        bool valid;
        uint8_t sinceFull;
        uint32_t offset;
        uint32_t lastEnd;
        uint32_t count;
        bool damaged;
        bool ioError;
        State() : valid(false), sinceFull(0u), offset(HeaderSize), lastEnd(HeaderSize),
                  count(0u), damaged(false), ioError(false)
        {
            for (uint8_t i = 0u; i < Fields; ++i)
            {
                value[i] = 0u;
            }
        }
    };
    /* [EN] Fixed, versioned header; it is not a stored CSV legend. */
    static const uint8_t *header(void)
    {
        static const uint8_t bytes[HeaderSize] = {'E', 'V', 'T', 'D', 1u, (uint8_t)Fields,
                                                  (uint8_t)Checkpoint, 0u};
        return bytes;
    }
    /* [EN] 1=binary, 0=legacy text, 2=empty or partial matching header, -1=unknown. */
    static int begin(File &file, State &state)
    {
        state = State();
        if (!file.seek(0u))
        {
            state.ioError = true;
            return -1;
        }
        uint8_t bytes[HeaderSize] = {};
        const size_t size = file.readBytes((char *)bytes, (size_t)HeaderSize);
        const size_t fileSize = file.size();
        const size_t expected = (fileSize < (size_t)HeaderSize) ? fileSize : (size_t)HeaderSize;
        /* [EN] A short read is an I/O problem, not an incomplete file. */
        if (size != expected)
        {
            state.ioError = true;
            return -1;
        }
        if ((size == (size_t)HeaderSize) && (memcmp(bytes, header(), (size_t)HeaderSize) == 0))
        {
            return 1;
        }
        if (!file.seek(0u))
        {
            state.ioError = true;
            return -1;
        }
        if ((size < (size_t)HeaderSize) && (memcmp(bytes, header(), size) == 0))
        {
            return 2;
        }
        if ((size > 0u) && ((bytes[0] == '#') || ((bytes[0] >= '0') && (bytes[0] <= '9'))))
        {
            return 0;
        }
        return -1;
    }
    static uint16_t crc(const uint8_t *bytes, size_t size)
    {
        uint16_t result = 0xffffu;
        for (size_t i = 0u; i < size; ++i)
        {
            result ^= (uint16_t)((uint16_t)bytes[i] << 8);
            for (uint8_t bit = 0u; bit < 8u; ++bit)
            {
                result = (result & 0x8000u) ? (uint16_t)((result << 1) ^ 0x1021u)
                                            : (uint16_t)(result << 1);
            }
        }
        return result;
    }
    /* [EN] Base-128 little-endian varint. */
    static size_t put(uint64_t value, uint8_t *out)
    {
        size_t used = 0u;
        do
        {
            out[used] = (uint8_t)(value & 127u);
            value >>= 7;
            if (value != 0u)
            {
                out[used] |= 128u;
            }
            ++used;
        } while (value != 0u);
        return used;
    }
    static bool get(const uint8_t *in, size_t size, size_t &cursor, uint64_t &value)
    {
        value = 0u;
        for (uint8_t shift = 0u; (shift < 35u) && (cursor < size); shift = (uint8_t)(shift + 7u))
        {
            const uint8_t byte = in[cursor++];
            value |= (uint64_t)(byte & 127u) << shift;
            if (!(byte & 128u))
            {
                return true;
            }
        }
        return false;
    }
    /* [EN] CSV row -> 22 numbers; exactly the public text form, nothing else. */
    static bool parse(const char *text, uint32_t *values)
    {
        const char *cursor = text;
        for (uint8_t i = 0u; i < (uint8_t)Fields; ++i)
        {
            uint64_t value = 0u;
            if ((cursor == NULL) || (*cursor < '0') || (*cursor > '9'))
            {
                return false;
            }
            while ((*cursor >= '0') && (*cursor <= '9'))
            {
                value = (value * 10u) + (uint64_t)(*cursor++ - '0');
                if (value > 0xFFFFFFFFull)
                {
                    return false;
                }
            }
            values[i] = (uint32_t)value;
            if ((i + 1u) < (uint8_t)Fields)
            {
                if (*cursor++ != ',')
                {
                    return false;
                }
            }
        }
        return (*cursor == '\0') || ((*cursor == '\r') && (cursor[1] == '\0')) ||
               ((*cursor == '\n') && (cursor[1] == '\0'));
    }
    /* [EN] State -> one CSV line (without the newline). 0 when it does not fit. */
    static size_t text(const State &state, char *out, size_t size)
    {
        size_t used = 0u;
        for (uint8_t i = 0u; i < (uint8_t)Fields; ++i)
        {
            if (used >= size)
            {
                return 0u;
            }
            const int written = snprintf(out + used, size - used, "%lu%c",
                                         (unsigned long)state.value[i],
                                         ((i + 1u) == (uint8_t)Fields) ? '\n' : ',');
            if ((written <= 0) || ((size_t)written >= (size - used)))
            {
                return 0u;
            }
            used += (size_t)written;
        }
        return used;
    }
    /* [EN] Encode one row; the caller's state changes only after a good write. */
    static size_t encode(State &state, const uint32_t *values, uint8_t *out)
    {
        uint8_t full[Fields * 5u];
        uint8_t delta[3u + Fields * 5u];
        size_t fullLen = 0u;
        size_t deltaLen = 3u;
        uint32_t mask = 0u;
        for (uint8_t i = 0u; i < (uint8_t)Fields; ++i)
        {
            fullLen += put((uint64_t)values[i], full + fullLen);
            if (values[i] != state.value[i])
            {
                mask |= (uint32_t)1u << i;
                const int64_t diff = (int64_t)values[i] - (int64_t)state.value[i];
                deltaLen += put((diff >= 0) ? ((uint64_t)diff * 2u)
                                            : ((uint64_t)(-diff) * 2u - 1u),
                                delta + deltaLen);
            }
        }
        for (uint8_t i = 0u; i < 3u; ++i)
        {
            delta[i] = (uint8_t)(mask >> (8u * i));
        }
        const bool useDelta = (state.valid != false) && ((uint8_t)(state.sinceFull + 1u) < (uint8_t)Checkpoint) &&
                              (deltaLen < fullLen);
        const size_t payload = useDelta ? deltaLen : fullLen;
        out[0] = 0xa5u;
        out[1] = 0xc3u;
        out[2] = (uint8_t)payload;
        out[3] = useDelta ? 1u : 0u;
        memcpy(out + 4u, useDelta ? delta : full, payload);
        const uint16_t check = crc(out + 2u, payload + 2u);
        out[4u + payload] = (uint8_t)check;
        out[5u + payload] = (uint8_t)(check >> 8);
        for (uint8_t i = 0u; i < (uint8_t)Fields; ++i)
        {
            state.value[i] = values[i];
        }
        state.valid = true;
        state.sinceFull = useDelta ? (uint8_t)(state.sinceFull + 1u) : 0u;
        return payload + 6u;
    }
    /* [EN] Read the next record, resyncing one byte at a time on corruption. */
    static bool next(File &file, State &state)
    {
        if (state.ioError)
        {
            return false;
        }
        const uint32_t size = (uint32_t)file.size();
        while (state.offset < size)
        {
            yield();
            const uint32_t start = state.offset;
            uint8_t frame[MaxFrame];
            if (!file.seek(start))
            {
                state.ioError = true;
                state.valid = false;
                return false;
            }
            const size_t got = file.readBytes((char *)frame, 4u);
            const size_t expected = ((size - start) < 4u) ? (size - start) : 4u;
            if (got != expected)
            {
                state.ioError = true;
                state.valid = false;
                return false;
            }
            if ((got < 4u) || (frame[0] != 0xa5u) || (frame[1] != 0xc3u) ||
                (frame[2] > (uint8_t)(Fields * 5u)) || (frame[3] > 1u))
            {
                state.damaged = true;
                state.valid = false;
                state.offset = start + 1u;
                continue;
            }
            const size_t payload = (size_t)frame[2];
            const size_t remaining = (size_t)(size - start - 4u);
            const size_t expectedPayload = (remaining < (payload + 2u)) ? remaining : (payload + 2u);
            const size_t gotPayload = file.readBytes((char *)frame + 4u, payload + 2u);
            if (gotPayload != expectedPayload)
            {
                state.ioError = true;
                state.valid = false;
                return false;
            }
            const uint16_t stored = (uint16_t)((uint16_t)frame[4u + payload] |
                                               ((uint16_t)frame[5u + payload] << 8));
            if ((gotPayload != (payload + 2u)) || (crc(frame + 2u, payload + 2u) != stored))
            {
                state.damaged = true;
                state.valid = false;
                state.offset = start + 1u;
                continue;
            }
            const bool isDelta = (frame[3] != 0u);
            uint32_t values[Fields];
            for (uint8_t i = 0u; i < (uint8_t)Fields; ++i)
            {
                values[i] = state.value[i];
            }
            size_t cursor = 0u;
            uint32_t mask = 0x3fffffu;
            bool good = true;
            if (isDelta)
            {
                if ((state.valid == false) || (state.sinceFull >= (uint8_t)(Checkpoint - 1u)) || (payload < 3u))
                {
                    good = false;
                }
                else
                {
                    mask = (uint32_t)frame[4] | ((uint32_t)frame[5] << 8) | ((uint32_t)frame[6] << 16);
                    if ((mask & ~0x3fffffu) != 0u)
                    {
                        good = false;
                    }
                    cursor = 3u;
                }
            }
            for (uint8_t i = 0u; (good != false) && (i < (uint8_t)Fields); ++i)
            {
                if ((mask & ((uint32_t)1u << i)) == 0u)
                {
                    continue;
                }
                uint64_t raw = 0u;
                if (!get(frame + 4u, payload, cursor, raw))
                {
                    good = false;
                    break;
                }
                const int64_t result = isDelta
                    ? ((int64_t)values[i] + (((raw & 1u) != 0u) ? (-(int64_t)(raw / 2u) - 1) : (int64_t)(raw / 2u)))
                    : (int64_t)raw;
                if ((result < 0) || (result > (int64_t)0xFFFFFFFFll))
                {
                    good = false;
                    break;
                }
                values[i] = (uint32_t)result;
            }
            state.offset = start + (uint32_t)payload + 6u;
            if ((good == false) || (cursor != payload))
            {
                state.valid = false;
                state.damaged = true;
                continue;
            }
            for (uint8_t i = 0u; i < (uint8_t)Fields; ++i)
            {
                state.value[i] = values[i];
            }
            state.valid = true;
            state.sinceFull = isDelta ? (uint8_t)(state.sinceFull + 1u) : 0u;
            state.lastEnd = state.offset;
            ++state.count;
            return true;
        }
        return false;
    }
};
#endif
