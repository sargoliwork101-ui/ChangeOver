/*
 * plink_log_codec.h - v1.138 bounded, lossless day-record codec.
 * Explicit byte order, no packed structs, heap allocations or floating point.
 * Every record is appended immediately with its own length, sync and CRC16.
 * At least every 16th record is full; damaged deltas wait for the next full row.
 */
#ifndef PLINK_LOG_CODEC_H
#define PLINK_LOG_CODEC_H
struct EspLogCodec {
    enum { Fields = 18, HeaderSize = 8, MaxFrame = 100, Checkpoint = 16 };
    struct State {
        uint32_t value[Fields] = {};
        bool valid = false;
        uint8_t sinceFull = 0u;
        uint32_t offset = HeaderSize;
        uint32_t lastEnd = HeaderSize;
        uint32_t count = 0u;
        bool damaged = false;
        bool ioError = false;
    };
    /* [EN] Header is fixed and versioned; it is not a stored CSV legend. */
    static const uint8_t *header() {
        static const uint8_t bytes[HeaderSize] = {'E','V','T','D',1,Fields,Checkpoint,0};
        return bytes;
    }
    /* [EN] 1=binary, 0=legacy text, 2=empty/partial matching header, -1=unknown. */
    static int begin(File &file, State &state) {
        state = State();
        if (!file.seek(0u)) { state.ioError=true; return -1; }
        uint8_t bytes[HeaderSize] = {};
        const size_t size = file.readBytes((char *)bytes, HeaderSize);
        const size_t expected=file.size()<(size_t)HeaderSize?file.size():(size_t)HeaderSize;
        /* [EN] A short READ is not an incomplete file: never erase on I/O failure. */
        if (size!=expected) { state.ioError=true; return -1; }
        if (size == HeaderSize && memcmp(bytes, header(), HeaderSize) == 0) return 1;
        if (!file.seek(0u)) { state.ioError=true; return -1; }
        if (size < HeaderSize && memcmp(bytes, header(), size) == 0) return 2;
        if (size && (bytes[0] == '#' || (bytes[0] >= '0' && bytes[0] <= '9'))) return 0;
        return -1;
    }
    static uint16_t crc(const uint8_t *bytes, size_t size) {
        uint16_t result = 0xffffu;
        for (size_t i=0u; i<size; ++i) {
            result ^= (uint16_t)((uint16_t)bytes[i] << 8);
            for (uint8_t b=0u; b<8u; ++b)
                result = (result & 0x8000u) ? (uint16_t)((result << 1) ^ 0x1021u) : (uint16_t)(result << 1);
        }
        return result;
    }
    static size_t put(uint64_t value, uint8_t *out) {
        size_t used = 0u;
        do {
            out[used] = (uint8_t)(value & 127u);
            value >>= 7;
            if (value) out[used] |= 128u;
            ++used;
        } while (value);
        return used;
    }
    static bool get(const uint8_t *in, size_t size, size_t &cursor, uint64_t &value) {
        value = 0u;
        for (uint8_t shift=0u; shift<35u && cursor<size; shift=(uint8_t)(shift+7u)) {
            const uint8_t byte = in[cursor++];
            value |= (uint64_t)(byte & 127u) << shift;
            if (!(byte & 128u)) return true;
        }
        return false;
    }
    static bool parse(const char *text, uint32_t *values) {
        for (uint8_t i=0u; i<Fields; ++i) {
            uint64_t value = 0u;
            if (*text<'0' || *text>'9') return false;
            while (*text>='0' && *text<='9') {
                value = value*10u + (uint8_t)(*text++-'0');
                if (value>UINT32_MAX) return false;
            }
            values[i] = (uint32_t)value;
            if (i+1u<Fields) { if (*text++!=',') return false; }
        }
        return *text=='\0' || (*text=='\n' && text[1]=='\0');
    }
    static size_t text(const State &state, char *out, size_t size) {
        size_t used = 0u;
        for (uint8_t i=0u; i<Fields; ++i) {
            if (used>=size) return 0u;
            const int n = snprintf(out+used,size-used,"%lu%c",(unsigned long)state.value[i],i+1u==Fields?'\n':',');
            if (n<=0 || (size_t)n>=size-used) return 0u;
            used += (size_t)n;
        }
        return used;
    }
    /* [EN] Candidate state changes only after its complete packet is persisted. */
    static size_t encode(State &state, const uint32_t *values, uint8_t *out) {
        uint8_t full[Fields*5u];
        uint8_t delta[3u+Fields*5u];
        size_t nf=0u, nd=3u;
        uint32_t mask=0u;
        for (uint8_t i=0u; i<Fields; ++i) {
            nf += put(values[i],full+nf);
            if (values[i]!=state.value[i]) {
                mask |= (uint32_t)1u<<i;
                const int64_t diff = (int64_t)values[i]-(int64_t)state.value[i];
                nd += put(diff>=0 ? (uint64_t)diff*2u : (uint64_t)(-diff)*2u-1u,delta+nd);
            }
        }
        for (uint8_t i=0u; i<3u; ++i) delta[i]=(uint8_t)(mask>>(8u*i));
        const bool useDelta = state.valid && state.sinceFull+1u<Checkpoint && nd<nf;
        const size_t payload = useDelta ? nd : nf;
        out[0]=0xa5u; out[1]=0xc3u; out[2]=(uint8_t)payload; out[3]=useDelta?1u:0u;
        memcpy(out+4u,useDelta?delta:full,payload);
        const uint16_t check = crc(out+2u,payload+2u);
        out[4u+payload]=(uint8_t)check; out[5u+payload]=(uint8_t)(check>>8);
        memcpy(state.value,values,sizeof(state.value));
        state.valid=true; state.sinceFull=useDelta?(uint8_t)(state.sinceFull+1u):0u;
        return payload+6u;
    }
    /* [EN] Scan/resync on corruption. Never decode a delta without a valid base. */
    static bool next(File &file, State &state) {
        if (state.ioError) return false;
        const uint32_t size = (uint32_t)file.size();
        while (state.offset<size) {
            yield();
            const uint32_t start = state.offset;
            uint8_t frame[MaxFrame] = {};
            if (!file.seek(start)) { state.ioError=true; state.valid=false; return false; }
            const size_t got = file.readBytes((char *)frame,4u);
            const size_t expected=size-start<4u?size-start:4u;
            if (got!=expected) { state.ioError=true; state.valid=false; return false; }
            if (got<4u || frame[0]!=0xa5u || frame[1]!=0xc3u || frame[2]>93u || frame[3]>1u) {
                state.damaged=true; state.valid=false; state.offset=start+1u; continue;
            }
            const size_t payload=frame[2];
            const size_t remaining=size-start-4u;
            const size_t expectedPayload=remaining<payload+2u?remaining:payload+2u;
            const size_t gotPayload=file.readBytes((char *)frame+4u,payload+2u);
            if (gotPayload!=expectedPayload) { state.ioError=true; state.valid=false; return false; }
            if (gotPayload!=payload+2u ||
                crc(frame+2u,payload+2u)!=(uint16_t)(frame[4u+payload]|((uint16_t)frame[5u+payload]<<8))) {
                state.damaged=true; state.valid=false; state.offset=start+1u; continue;
            }
            const bool isDelta=frame[3]!=0u;
            uint32_t values[Fields];
            memcpy(values,state.value,sizeof(values));
            size_t cursor=0u;
            uint32_t mask=0x3ffffu;
            bool good=true;
            if (isDelta) {
                if (!state.valid || state.sinceFull>=Checkpoint-1u || payload<3u) good=false;
                else {
                    mask=(uint32_t)frame[4]|((uint32_t)frame[5]<<8)|((uint32_t)frame[6]<<16);
                    if (mask & ~0x3ffffu) good=false;
                    cursor=3u;
                }
            }
            for (uint8_t i=0u; good && i<Fields; ++i) {
                if (!(mask & ((uint32_t)1u<<i))) continue;
                uint64_t value;
                if (!get(frame+4u,payload,cursor,value)) { good=false; break; }
                const int64_t result = isDelta ? (int64_t)values[i] + ((value&1u)?-(int64_t)(value/2u)-1:(int64_t)(value/2u)) : (int64_t)value;
                if (result<0 || result>UINT32_MAX) { good=false; break; }
                values[i]=(uint32_t)result;
            }
            state.offset=start+(uint32_t)payload+6u;
            if (!good || cursor!=payload) { state.valid=false; state.damaged=true; continue; }
            memcpy(state.value,values,sizeof(values));
            state.valid=true; state.sinceFull=isDelta?(uint8_t)(state.sinceFull+1u):0u;
            state.lastEnd=state.offset; ++state.count;
            return true;
        }
        return false;
    }
};
#endif
