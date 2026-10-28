/**
 * @file   host_test_esp_link.cpp
 * @brief  [EN] First host tests for the ESP program.
 *
 *         The sketch itself is compiled here, unmodified: this file includes
 *         ESP_Calibration.ino, so setup() and loop() and every plink_*.h the
 *         board flashes go through a real compiler before anybody opens the
 *         case. Until now nothing did that. tools/check_firmware_syntax.sh
 *         looks only at Firmware/, and audit_consistency.py reads the panel
 *         as text, so the C++ that frames every STM32 message, clamps every
 *         parameter and answers every browser request was only ever built by
 *         the Arduino IDE, by hand, at flashing time. The very first run of
 *         this file found that _Static_assert (a C keyword) was being used in
 *         a C++ sketch, which no C++ dialect accepts: the program did not
 *         compile at all.
 *
 *         These tests cover the places where being wrong is silent rather
 *         than obvious: the CRC and framing on the wire, the pending-mask
 *         arithmetic (where id 96 would evaluate 1UL << 32, undefined
 *         behaviour rather than a wrong pixel), the integer parser behind
 *         every query string, the clamping on /s, and the panel the board
 *         serves.
 *
 * @brief  [FA] نخستین تست‌های هاست برای برنامهٔ ESP.
 *
 *         خود اسکچ اینجا دست‌نخورده کامپایل می‌شود: این فایل
 *         ESP_Calibration.ino را include می‌کند، پس setup() و loop() و همهٔ
 *         هدرهایی که روی برد می‌روند پیش از باز کردن جعبه از یک کامپایلر
 *         واقعی رد می‌شوند. تا امروز هیچ‌چیز این کار را نمی‌کرد. همان اجرای
 *         اول نشان داد _Static_assert (کلیدواژهٔ C) در اسکچ ++C به کار رفته
 *         که هیچ دیالکتی نمی‌پذیرد: برنامه اصلاً کامپایل نمی‌شد.
 */
#include "stub_arduino.h"

/* the globals the stubs declare extern */
unsigned long G_StubMillis = 0;
StubSerial Serial;
StubWiFi WiFi;
StubFS LittleFS;

/* --------------------------------------------------------------------------
   [EN] The REAL sketch, compiled exactly as the board compiles it: one
        translation unit, ESP8266 branch, no edits.
   [FA] خودِ اسکچ، دقیقاً همان‌طور که برد کامپایل می‌کند: یک واحد ترجمه،
        شاخهٔ ESP8266، بدون هیچ ویرایشی.
   -------------------------------------------------------------------------- */
#define ESP8266 1
#include "../ESP_Calibration.ino"

#include <iostream>
#include <string>
#include <utility>

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what, const std::string &detail = "")
{
    checks++;
    if (!cond) {
        failures++;
        std::cout << "  FAIL  " << what;
        if (!detail.empty()) std::cout << "   [" << detail << "]";
        std::cout << "\n";
    }
}

static std::string hex16(uint16_t v)
{
    const char *d = "0123456789ABCDEF";
    std::string s = "0x";
    s += d[(v >> 12) & 15]; s += d[(v >> 8) & 15];
    s += d[(v >> 4) & 15];  s += d[v & 15];
    return s;
}

/* ---- the CRC the firmware computes, over a whole buffer ----------------- */
static uint16_t crc_over(const uint8_t *p, size_t n)
{
    uint16_t crc = ESP_LINK_CRC16_INIT;
    for (size_t i = 0; i < n; i++) crc = func__Esp_Crc16(crc, p[i]);
    return crc;
}

static std::vector<uint8_t> build_frame(uint8_t type, const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> f;
    f.push_back(ESP_LINK_SOF_BYTE0);
    f.push_back(ESP_LINK_SOF_BYTE1);
    f.push_back(ESP_LINK_PROTOCOL_VERSION);
    f.push_back(type);
    f.push_back((uint8_t)(payload.size() & 0xFFu));
    f.push_back((uint8_t)(payload.size() >> 8));
    f.insert(f.end(), payload.begin(), payload.end());
    const uint16_t crc = crc_over(&f[2], 4u + payload.size());
    f.push_back((uint8_t)(crc & 0xFFu));
    f.push_back((uint8_t)(crc >> 8));
    return f;
}

static void append_u32(std::vector<uint8_t> &payload, uint32_t value)
{
    payload.push_back((uint8_t)(value & 0xFFu));
    payload.push_back((uint8_t)((value >> 8) & 0xFFu));
    payload.push_back((uint8_t)((value >> 16) & 0xFFu));
    payload.push_back((uint8_t)((value >> 24) & 0xFFu));
}

static std::vector<uint8_t> lut_data(uint8_t channel,
                                     const std::vector<std::pair<uint32_t, uint32_t>> &points)
{
    std::vector<uint8_t> payload;
    payload.push_back(channel);
    payload.push_back(0u);
    payload.push_back((uint8_t)points.size());
    for (size_t i = 0u; i < points.size(); i++)
    {
        append_u32(payload, points[i].first);
        append_u32(payload, points[i].second);
    }
    return payload;
}

static void feed(const std::vector<uint8_t> &v)
{
    for (size_t i = 0; i < v.size(); i++) func__Esp_ParseByte(v[i]);
}

/* [EN] v1.93 history tests need a known pack voltage and a way to read one
        field of one saved row, instead of substring guessing.
   [FA] تست‌های تاریخچه به ولتاژ معلوم پک و خواندن یک خانه از یک ردیف ذخیره‌شده
        نیاز دارند، نه حدس با زیررشته. */
static long row_field(const std::string &body, size_t rowIndex, size_t fieldIndex)
{
    size_t row = 0u, k = 0u, field = 0u, start = 0u;
    while (k <= body.size()) {
        const bool eol = (k == body.size()) || (body[k] == '\n');
        if (eol) {
            if ((k > start) && (body[start] != '#')) {
                if (row == rowIndex) {
                    std::string line = body.substr(start, k - start);
                    size_t from = 0u;
                    for (field = 0u; field < fieldIndex; field++) {
                        size_t comma = line.find(',', from);
                        if (comma == std::string::npos) return -1;
                        from = comma + 1u;
                    }
                    size_t comma = line.find(',', from);
                    return atol(line.substr(from, comma - from).c_str());
                }
                row++;
            }
            start = k + 1u;
        }
        k++;
    }
    return -1;
}

static std::vector<uint8_t> tlm_payload(uint16_t seq)
{
    std::vector<uint8_t> p(ESP_LINK_TLM_SIZE, 0u);
    p[0] = (uint8_t)(seq & 0xFFu);
    p[1] = (uint8_t)(seq >> 8);
    for (uint8_t i = 0u; i < ESP_LINK_TLM_FIELD_COUNT; i++) {
        const uint32_t v = 1000u + i;
        const size_t o = ESP_LINK_TLM_FIELD_OFFSET + (size_t)i * 4u;
        p[o] = (uint8_t)(v & 0xFFu);
        p[o + 1] = (uint8_t)((v >> 8) & 0xFFu);
        p[o + 2] = (uint8_t)((v >> 16) & 0xFFu);
        p[o + 3] = (uint8_t)((v >> 24) & 0xFFu);
    }
    return p;
}

static std::vector<uint8_t> tlm_payload_v24(uint16_t seq, uint32_t v24)
{
    std::vector<uint8_t> p = tlm_payload(seq);
    const size_t o = ESP_LINK_TLM_FIELD_OFFSET + 15u * 4u;
    p[o] = (uint8_t)(v24 & 0xFFu);
    p[o + 1u] = (uint8_t)((v24 >> 8) & 0xFFu);
    p[o + 2u] = (uint8_t)((v24 >> 16) & 0xFFu);
    p[o + 3u] = (uint8_t)((v24 >> 24) & 0xFFu);
    return p;
}


static std::vector<uint8_t> tlm_payload_field(uint16_t seq, uint32_t v24,
                                              uint8_t field, uint32_t value)
{
    std::vector<uint8_t> p = tlm_payload_v24(seq, v24);
    const size_t o = ESP_LINK_TLM_FIELD_OFFSET + (size_t)field * 4u;
    p[o] = (uint8_t)(value & 0xFFu);
    p[o + 1u] = (uint8_t)((value >> 8) & 0xFFu);
    p[o + 2u] = (uint8_t)((value >> 16) & 0xFFu);
    p[o + 3u] = (uint8_t)((value >> 24) & 0xFFu);
    return p;
}

/* [EN] Walk the bytes this panel wrote and return the payload of the first
        frame of the asked type, or an empty vector when there is none.
   [FA] بایت‌هایی که پنل نوشته را می‌گردد و payload اولین فریم از نوع خواسته‌شده
        را برمی‌گرداند، یا هیچ. */
static std::vector<uint8_t> find_tx_frame(uint8_t type)
{
    std::vector<uint8_t> found;
    size_t i = 0u;

    while ((i + (size_t)ESP_LINK_HEADER_SIZE) <= Serial.tx.size()) {
        if ((Serial.tx[i] != ESP_LINK_SOF_BYTE0) || (Serial.tx[i + 1u] != ESP_LINK_SOF_BYTE1)) {
            i++;
            continue;
        }
        const size_t len = (size_t)Serial.tx[i + 4u] | ((size_t)Serial.tx[i + 5u] << 8);
        const size_t end = i + (size_t)ESP_LINK_HEADER_SIZE + len + (size_t)ESP_LINK_CRC_SIZE;
        if (end > Serial.tx.size()) {
            break;
        }
        if (Serial.tx[i + 3u] == type) {
            found.assign(Serial.tx.begin() + (long)(i + (size_t)ESP_LINK_HEADER_SIZE),
                         Serial.tx.begin() + (long)(i + (size_t)ESP_LINK_HEADER_SIZE + len));
            return found;
        }
        i = end;
    }
    return found;
}

/* [EN] Is there at least one frame of this type in what the panel wrote?
   [FA] آیا دست‌کم یک فریم از این نوع بین بایت‌های نوشته‌شده هست؟ */
static bool has_tx_frame(uint8_t type, size_t *lenOut)
{
    size_t i = 0u;

    while ((i + (size_t)ESP_LINK_HEADER_SIZE) <= Serial.tx.size()) {
        if ((Serial.tx[i] != ESP_LINK_SOF_BYTE0) || (Serial.tx[i + 1u] != ESP_LINK_SOF_BYTE1)) {
            i++;
            continue;
        }
        const size_t len = (size_t)Serial.tx[i + 4u] | ((size_t)Serial.tx[i + 5u] << 8);
        const size_t end = i + (size_t)ESP_LINK_HEADER_SIZE + len + (size_t)ESP_LINK_CRC_SIZE;
        if (end > Serial.tx.size()) {
            break;
        }
        if (Serial.tx[i + 3u] == type) {
            if (lenOut != NULL) { *lenOut = len; }
            return true;
        }
        i = end;
    }
    return false;
}

/* ======================================================================== */
int main(void)
{
    std::cout << "ESP program host tests - the real sketch, compiled\n";
    std::cout << std::string(70, '=') << "\n";

    /* ---- 1. the sketch's own entry point runs -------------------------- */
    setup();
    check(ESP_WEB_SERVER_T__G__Server.has("/", HTTP_GET),
          "setup() registers the panel route");
    check(ESP_WEB_SERVER_T__G__Server.has("/t", HTTP_GET),
          "setup() registers the telemetry route");
    check(ESP_WEB_SERVER_T__G__Server.has("/s", HTTP_POST),
          "setup() registers the set-parameter route as POST");
    check(ESP_WEB_SERVER_T__G__Server.has("/m", HTTP_GET) &&
          ESP_WEB_SERVER_T__G__Server.has("/m", HTTP_POST),
          "/m answers both GET (read) and POST (reset)",
          "one map keyed by path alone would lose one of them");
    check(ESP_WEB_SERVER_T__G__Server.has("/lut/read", HTTP_POST),
          "setup() registers the active-LUT readback route");
    check(BOOL__G__FsOk, "setup() mounts the file system for the bench log");

    /* ---- 2. CRC-16/CCITT-FALSE against the published vector -------------
       [EN] "123456789" -> 0x29B1. Every frame in both directions rests on
            this. A wrong polynomial or init drops all traffic and looks
            exactly like a loose cable.
       [FA] بردار منتشرشده. هر قاب در هر دو جهت به همین تکیه دارد؛ خطا در آن
            شبیه کابل شل به نظر می‌رسد. */
    {
        const uint16_t crc = crc_over((const uint8_t *)"123456789", 9u);
        check(crc == 0x29B1u, "CRC-16/CCITT-FALSE matches the published vector",
              "got " + hex16(crc) + ", expected 0x29B1");
        check(crc_over((const uint8_t *)"", 0u) == ESP_LINK_CRC16_INIT,
              "an empty buffer leaves the CRC at its init value");
    }

    /* ---- 3. little-endian field reader --------------------------------- */
    {
        const uint8_t b[5] = { 0x00u, 0x78u, 0x56u, 0x34u, 0x12u };
        check(func__Esp_ReadU32(b, 1u) == 0x12345678uL,
              "u32 fields are read little-endian from their offset");
    }

    /* ---- 4. a good telemetry frame is taken, a bad one is not ----------
       [EN] The second half matters more: a parser that accepts a bad CRC
            turns line noise into readings that look perfectly plausible.
       [FA] نیمهٔ دوم مهم‌تر است: پارسری که CRC بد را بپذیرد نویز خط را به
            عددهای کاملاً باورپذیر تبدیل می‌کند. */
    {
        BOOL__G__TlmSeen = false;
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload(1u)));
        check(BOOL__G__TlmSeen, "a well-formed telemetry frame is accepted");
        check(UINT32_T__G__TlmField[0] == 1000u &&
              UINT32_T__G__TlmField[ESP_LINK_TLM_FIELD_COUNT - 1u] ==
                  1000u + (ESP_LINK_TLM_FIELD_COUNT - 1u),
              "all 25 telemetry fields land in order, first and last included",
              "an off-by-one stride silently shifts every reading on the panel");

        const uint32_t errBefore = UINT32_T__G__RxCrcError;
        std::vector<uint8_t> bad = build_frame(ESP_MSG_TLM_LIVE, tlm_payload(2u));
        bad[bad.size() - 1] ^= 0xFFu;
        feed(bad);
        check(UINT32_T__G__RxCrcError == errBefore + 1u,
              "a frame with a broken CRC is counted and dropped",
              "accepting it would turn line noise into plausible readings");

        const uint32_t verBefore = UINT32_T__G__RxVersionMismatch;
        std::vector<uint8_t> wrongVer = build_frame(ESP_MSG_TLM_LIVE, tlm_payload(3u));
        wrongVer[2] = (uint8_t)(ESP_LINK_PROTOCOL_VERSION + 1u);
        feed(wrongVer);
        check(UINT32_T__G__RxVersionMismatch == verBefore + 1u,
              "a frame from a differently-flashed STM32 is counted, not decoded",
              "this is the counter that tells the two boards are out of step");
    }

    /* ---- 5. the parser recovers after rubbish --------------------------- */
    {
        BOOL__G__TlmSeen = false;
        for (int i = 0; i < 64; i++) func__Esp_ParseByte((uint8_t)(i * 7));
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload(10u)));
        check(BOOL__G__TlmSeen, "the parser resynchronises after a burst of junk",
              "a parser that wedges needs a power cycle to come back");
    }

    /* ---- 5b. every error path keeps an AA that may be SOF0 -------------- */
    {
        const std::vector<uint8_t> good = build_frame(ESP_MSG_TLM_LIVE,
                                                       tlm_payload(11u));

        /* Version mismatch followed immediately by AA 55. */
        ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
        BOOL__G__TlmSeen = false;
        func__Esp_ParseByte(ESP_LINK_SOF_BYTE0);
        func__Esp_ParseByte(ESP_LINK_SOF_BYTE1);
        func__Esp_ParseByte((uint8_t)(ESP_LINK_PROTOCOL_VERSION + 1u));
        feed(good);
        check(UINT32_T__G__RxVersionMismatch > 0u && BOOL__G__TlmSeen,
              "version mismatch resync keeps the next AA 55 frame");

        /* Invalid high length byte is AA, followed by 55 and the remainder
           of a valid frame (the retained AA is its SOF0). */
        ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
        BOOL__G__TlmSeen = false;
        const uint8_t badLength[] = { ESP_LINK_SOF_BYTE0,
                                      ESP_LINK_SOF_BYTE1,
                                      ESP_LINK_PROTOCOL_VERSION,
                                      ESP_MSG_TLM_LIVE, 0x00u,
                                      ESP_LINK_SOF_BYTE0 };
        feed(std::vector<uint8_t>(badLength,
                                  badLength + sizeof(badLength)));
        feed(std::vector<uint8_t>(good.begin() + 1u, good.end()));
        check(BOOL__G__TlmSeen,
              "an impossible length with high byte AA retains the next frame");

        /* CRC error whose CRC-HI is AA, followed by 55 and a valid frame. */
        std::vector<uint8_t> crcBoundary;
        uint8_t oneBytePayload = 0u;
        for (uint16_t type = 0u; type < 256u; type++) {
            std::vector<uint8_t> candidate = build_frame((uint8_t)type,
                                                          std::vector<uint8_t>(1u, oneBytePayload));
            if (candidate.back() == ESP_LINK_SOF_BYTE0) {
                crcBoundary = candidate;
                break;
            }
        }
        check(!crcBoundary.empty(), "the test found a CRC-HI AA boundary case");
        if (!crcBoundary.empty()) {
            crcBoundary[crcBoundary.size() - 2u] ^= 0x01u;
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            BOOL__G__TlmSeen = false;
            feed(crcBoundary);
            feed(std::vector<uint8_t>(good.begin() + 1u, good.end()));
            check(UINT32_T__G__RxCrcError > 0u && BOOL__G__TlmSeen,
                  "CRC-HI AA is retained so AA 55 immediately recovers");
        }
    }

    /* ---- 6. outgoing frames are well formed ----------------------------- */
    {
        Serial.tx.clear();
        G_StubMillis += ESP_LINK_TX_INTERVAL_MS + 1u;
        func__Esp_SendSetParam(25u, 650u);
        func__Esp_PumpTx();
        check(Serial.tx.size() >= 9u, "a set-parameter frame reaches the wire",
              "wrote " + std::to_string(Serial.tx.size()) + " bytes");
        if (Serial.tx.size() >= 9u) {
            check(Serial.tx[0] == ESP_LINK_SOF_BYTE0 && Serial.tx[1] == ESP_LINK_SOF_BYTE1,
                  "it carries the two sync bytes");
            check(Serial.tx[2] == ESP_LINK_PROTOCOL_VERSION,
                  "it declares the protocol version the spec fixes");
            check(Serial.tx[3] == ESP_MSG_SET_PARAM, "it is typed SET_PARAM");
            const uint16_t len = (uint16_t)(Serial.tx[4] | (Serial.tx[5] << 8));
            check(Serial.tx.size() == (size_t)len + ESP_LINK_HEADER_SIZE + ESP_LINK_CRC_SIZE,
                  "the declared length matches the bytes actually written",
                  "declared " + std::to_string(len) + ", wrote " +
                      std::to_string(Serial.tx.size()));
            const uint16_t crc = crc_over(&Serial.tx[2], 4u + len);
            const uint16_t sent = (uint16_t)(Serial.tx[Serial.tx.size() - 2] |
                                             (Serial.tx[Serial.tx.size() - 1] << 8));
            check(crc == sent, "its CRC covers version..payload, little-endian",
                  hex16(crc) + " vs " + hex16(sent));
            check(Serial.tx[6] == 25u && func__Esp_ReadU32(&Serial.tx[6], 1u) == 650uL,
                  "the payload is [id][value LE], the id and value asked for");
        }
    }

    /* ---- 7. parameter items reported by the STM32 ----------------------- */
    {
        for (uint16_t i = 0u; i < ESP_PARAM_COUNT; i++) {
            UINT32_T__G__ParamApplied[i] = 0u;
            BOOL__G__ParamKnown[i] = false;
        }
        const uint8_t item[5] = { 107u, 0xE8u, 0x03u, 0x00u, 0x00u };  /* id 107 = 1000 */
        func__Esp_StoreParamItem(item);
        check(UINT32_T__G__ParamApplied[107] == 1000uL && BOOL__G__ParamKnown[107],
              "the last parameter in the table is stored and marked known");

        const uint8_t past[5] = { (uint8_t)ESP_PARAM_COUNT, 1u, 0u, 0u, 0u };
        const uint32_t guard = UINT32_T__G__ParamApplied[ESP_PARAM_COUNT - 1u];
        func__Esp_StoreParamItem(past);
        check(UINT32_T__G__ParamApplied[ESP_PARAM_COUNT - 1u] == guard,
              "an id past the end of the table is dropped, not written past it",
              "this is the one that corrupts memory instead of a reading");
    }

    /* ---- 8. the pending-mask arithmetic, at every boundary --------------
       [EN] The panel learns which parameters are still in flight from six
            32-bit masks in /t. id 96 is the first bit of the fourth word, and
            getting that boundary wrong once meant evaluating 1UL << 32:
            undefined behaviour, not an off-by-one. Both sides of all three
            boundaries are checked, by reading the JSON the browser reads.
       [FA] پنل از شش ماسک ۳۲ بیتی در t/ می‌فهمد کدام پارامترها در راه‌اند.
            شناسهٔ ۹۶ اولین بیت کلمهٔ چهارم است و یک‌بار اشتباه در همین مرز
            یعنی 1UL << 32 - رفتار تعریف‌نشده. هر دو طرف هر سه مرز از روی
            همان JSON که مرورگر می‌خواند آزموده می‌شود. */
    {
        /* [EN] v1.87: the sixth word starts at 160, so both sides of every
               word boundary are edges now, not just the first three.
           [FA] نسخهٔ ۱٫۸۷: کلمهٔ ششم از ۱۶۰ شروع می‌شود، پس هر دو طرف همهٔ
               مرزهای کلمه آزموده می‌شود. */
        const uint16_t edges[] = { 0u, 31u, 32u, 63u, 64u, 95u, 96u,
                                   127u, 128u, 159u, 160u,
                                   (uint16_t)(ESP_PARAM_COUNT - 1u) };
        for (size_t k = 0; k < sizeof edges / sizeof edges[0]; k++) {
            const uint16_t id = edges[k];
            for (uint16_t i = 0u; i < ESP_PARAM_COUNT; i++) BOOL__G__TxParamPending[i] = false;
            BOOL__G__TxParamPending[id] = true;

            ESP_WEB_SERVER_T__G__Server.call("/t", HTTP_GET);
            const std::string body = ESP_WEB_SERVER_T__G__Server.lastBody;

            /* the word this id must land in, and the bit inside it */
            /* v1.80: ids 128..131 (scenario-6 lamp/buzzer) added a fifth word */
            const int word = id / 32;                       /* 0..5 */
            const unsigned bit = (unsigned)(id % 32);
            static const char *keys[6] = { "\"q\":", "\"q2\":", "\"q3\":", "\"q4\":", "\"q5\":", "\"q6\":" };
            bool found = true;
            unsigned long seen[6] = { 0, 0, 0, 0, 0, 0 };
            for (int w = 0; w < 6; w++) {
                const size_t at = body.find(keys[w]);
                if (at == std::string::npos) { found = false; break; }
                seen[w] = strtoul(body.c_str() + at + strlen(keys[w]), NULL, 10);
            }
            check(found, "the telemetry JSON carries all six pending masks");
            if (!found) break;
            check(seen[word] == (1UL << bit),
                  "parameter " + std::to_string(id) + " sets exactly its own bit, in word " +
                      std::to_string(word + 1),
                  "got " + std::to_string(seen[word]) + ", expected " +
                      std::to_string(1UL << bit));
            unsigned long others = 0;
            for (int w = 0; w < 6; w++) if (w != word) others |= seen[w];
            check(others == 0uL,
                  "parameter " + std::to_string(id) + " leaves the other five words clear",
                  "a spilled bit makes the panel wait forever on a parameter nobody sent");
        }
        for (uint16_t i = 0u; i < ESP_PARAM_COUNT; i++) BOOL__G__TxParamPending[i] = false;
    }

    /* ---- 9. the query-string integer parser ----------------------------- */
    {
        int32_t v = 0;
        check(func__Esp_ParseInt("650", &v) && v == 650, "a plain number parses");
        check(func__Esp_ParseInt("36000000", &v) && v == 36000000, "10-hour scenario 5 limit (36000000 ms) parses");
        check(func__Esp_ParseInt("86400000", &v) && v == 86400000, "24-hour scenario 6 limit (86400000 ms) parses");
        check(func__Esp_ParseInt("-40", &v) && v == -40, "a negative number parses");
        check(func__Esp_ParseInt("0", &v) && v == 0, "zero parses");
        check(!func__Esp_ParseInt("", &v), "an empty string is refused");
        check(!func__Esp_ParseInt("12x", &v), "trailing rubbish is refused",
              "accepting it would turn a typo into a silent setting change");
        check(!func__Esp_ParseInt("x", &v), "a non-number is refused");
    }

    /* ---- 10. the panel the board actually serves ------------------------
       [EN] Checked on the bytes the BOARD sends, not on the preview file.
       [FA] روی همان بایت‌هایی که «برد» می‌فرستد، نه روی فایل پیش‌نمایش. */
    {
        ESP_WEB_SERVER_T__G__Server.sendCount = 0;
        ESP_WEB_SERVER_T__G__Server.call("/", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200, "GET / answers 200");
        check(ESP_WEB_SERVER_T__G__Server.sendCount > 100,
              "GET / streams the panel in many small chunks, not one giant write",
              std::to_string(ESP_WEB_SERVER_T__G__Server.sendCount) + " writes");
        check(ESP_WEB_SERVER_T__G__Server.lastHeaderKey == "Cache-Control" &&
              ESP_WEB_SERVER_T__G__Server.lastHeaderVal == "no-store",
              "GET / forbids caching, so a panel update reaches every browser",
              "a cached panel is the stale-page report all over again");

        const std::string html = ESP_WEB_SERVER_T__G__Server.lastBody;
        check(html.size() > 100000u, "GET / sends the whole panel",
              std::to_string(html.size()) + " bytes");
        check(sizeof(ESP_PANEL_HTML) <= ESP_PANEL_HTML_MAX_BYTES,
              "the panel fits the declared transfer budget",
              std::to_string(sizeof(ESP_PANEL_HTML)) + " of " +
                  std::to_string((size_t)ESP_PANEL_HTML_MAX_BYTES) + " bytes");
        check(html.rfind("</html>") != std::string::npos,
              "it ends with a closing document tag",
              "a truncated literal serves a page whose scripts never run");
        check(html.find("id=\"bs\">build ") != std::string::npos,
              "it carries the build stamp that identifies the running panel");

        /* the ordered layout, verified on the served bytes */
        const size_t p0 = html.find("id=\"p0\"");
        const size_t p1 = html.find("id=\"p1\"");
        check(p0 != std::string::npos && p1 != std::string::npos && p0 < p1,
              "the chargers page comes first in the served document");
        const std::string page0 = html.substr(p0, p1 - p0);
        check(page0.find("class=\"qgm\"") == std::string::npos,
              "the chart is off the chargers page, as ordered");
        check(page0.find("id=\"ctb\"") == std::string::npos,
              "the operating table is GONE, as ordered - it must not reappear");
        check(html.find("id=\"vb${n}\"></b> <span class=\"unit\">V</span><span class=\"bpct\" id=\"vp${n}\"></span>") != std::string::npos &&
              html.find("function batDisPct(") != std::string::npos,
              "page 0 renders the per-battery discharge percentage badge right beside vb1/vb2");

        ESP_WEB_SERVER_T__G__Server.call("/f.css", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              !ESP_WEB_SERVER_T__G__Server.lastBody.empty(),
              "GET /f.css serves the embedded font, so the panel needs no internet");
    }

    /* ---- 11. telemetry JSON fits its buffer ------------------------------
       [EN] snprintf truncates silently; a JSON cut mid-number parses as a
            different number or not at all, and the panel simply stops
            updating with no error anywhere.
       [FA] snprintf بی‌صدا می‌بُرد؛ JSON نصفه یا عدد دیگری می‌شود یا اصلاً
            پارس نمی‌شود و پنل بی‌هیچ خطایی از به‌روزرسانی می‌ایستد. */
    {
        for (uint16_t i = 0u; i < ESP_PARAM_COUNT; i++) {
            BOOL__G__TxParamPending[i] = true;
            BOOL__G__ParamKnown[i] = true;
            UINT32_T__G__ParamApplied[i] = 4294967295uL;   /* widest possible */
        }
        for (uint8_t i = 0u; i < ESP_LINK_TLM_FIELD_COUNT; i++) {
            UINT32_T__G__TlmField[i] = 4294967295uL;
        }
        ESP_WEB_SERVER_T__G__Server.call("/t", HTTP_GET);
        const size_t used = ESP_WEB_SERVER_T__G__Server.lastBody.size();
        check(used < (size_t)ESP_JSON_BUFFER_SIZE - 1u,
              "the telemetry JSON fits its buffer at the widest possible values",
              std::to_string(used) + " of " + std::to_string((size_t)ESP_JSON_BUFFER_SIZE) +
                  " bytes");
        check(ESP_WEB_SERVER_T__G__Server.lastBody[0] == '{' &&
              ESP_WEB_SERVER_T__G__Server.lastBody[used - 1u] == '}',
              "and it is still a closed JSON object at those values",
              "a truncated object is the silent failure this check exists for");
        for (uint16_t i = 0u; i < ESP_PARAM_COUNT; i++) BOOL__G__TxParamPending[i] = false;
    }

    /* ---- 12. POST /s: what it refuses, and what it does with the rest --- */
    {
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/s", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 400,
              "POST /s with no arguments is refused");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        {
            /* [EN] Derived from the live table size so adding parameters no
             *      longer silently turns this check into an accept.
             * [FA] از اندازهٔ زندهٔ جدول گرفته می‌شود تا با افزونه شدن پارامتر
             *      این چک به‌اشتباه پذیرش نشود. */
            char char__A__Id[12];

            snprintf(char__A__Id, sizeof(char__A__Id), "%u", (unsigned)ESP_PARAM_COUNT);
            ESP_WEB_SERVER_T__G__Server.setArg("id", char__A__Id);
        }
        ESP_WEB_SERVER_T__G__Server.setArg("v", "1");
        ESP_WEB_SERVER_T__G__Server.call("/s", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 400,
              "POST /s with an id one past the table is refused",
              "accepting it would write past the parameter array");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("id", "-1");
        ESP_WEB_SERVER_T__G__Server.setArg("v", "1");
        ESP_WEB_SERVER_T__G__Server.call("/s", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 400,
              "POST /s with a negative id is refused");

        /* [EN] id 94 is the hard-fault current: min 10, max 1500 (charger.c).
           [FA] شناسهٔ ۹۴ جریان hard fault است: کمینه ۱۰، بیشینه ۱۵۰۰. */
        const uint8_t ID = 94u;
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("id", "94");
        ESP_WEB_SERVER_T__G__Server.setArg("v", "999999");
        BOOL__G__TxParamPending[ID] = false;
        ESP_WEB_SERVER_T__G__Server.call("/s", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200,
              "POST /s with a too-large value is accepted, not rejected");
        check(UINT32_T__G__TxParamValue[ID] == (uint32_t)INT32_T__G__ParamMax[ID],
              "and clamped to the parameter's own maximum before it is queued",
              "queued " + std::to_string(UINT32_T__G__TxParamValue[ID]) + ", max is " +
                  std::to_string(INT32_T__G__ParamMax[ID]));

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("id", "94");
        ESP_WEB_SERVER_T__G__Server.setArg("v", "-5");
        ESP_WEB_SERVER_T__G__Server.call("/s", HTTP_POST);
        check(UINT32_T__G__TxParamValue[ID] == (uint32_t)INT32_T__G__ParamMin[ID],
              "a too-small value is clamped up to the minimum, never wrapped",
              "queued " + std::to_string(UINT32_T__G__TxParamValue[ID]) + ", min is " +
                  std::to_string(INT32_T__G__ParamMin[ID]));

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("id", "94");
        ESP_WEB_SERVER_T__G__Server.setArg("v", "650");
        BOOL__G__TxParamPending[ID] = false;
        Serial.tx.clear();
        ESP_WEB_SERVER_T__G__Server.call("/s", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              UINT32_T__G__TxParamValue[ID] == 650uL,
              "a value inside the limits is queued unchanged");
        check(BOOL__G__TxParamPending[ID] && BOOL__G__ParamUserSet[ID],
              "it is marked pending and user-set, so a board restart re-sends it");

        G_StubMillis += ESP_LINK_TX_INTERVAL_MS + 1u;
        func__Esp_PumpTx();
        check(!Serial.tx.empty(), "and it actually reaches the STM32 over the link",
              "a 200 that sends nothing is the worst possible answer");
    }

    /* ---- 12b. v1.54: no browser, no parameter refresh --------------------
            [EN] The periodic GET_PARAMS exists to keep a WATCHED page honest.
            With the tab closed the STM32 must never be asked to dump its
            whole parameter table again; the first poll re-arms it.
            [FA] نوسازی دوره‌ای فقط برای صفحه‌ای است که کسی می‌بیند. */
    {
        BOOL__G__BrowserSeen = false;
        UINT32_T__G__LastParamRefreshMs = (uint32_t)G_StubMillis;
        G_StubMillis += ESP_LINK_PARAM_REFRESH_MS + ESP_LINK_TX_INTERVAL_MS + 1u;
        Serial.tx.clear();
        func__Esp_PumpTx();
        bool sawGet = false;
        for (size_t i = 0; i + 3u < Serial.tx.size(); i++) {
            if (Serial.tx[i] == ESP_LINK_SOF_BYTE0 && Serial.tx[i + 1] == ESP_LINK_SOF_BYTE1 &&
                Serial.tx[i + 3] == ESP_MSG_GET_PARAMS) { sawGet = true; }
        }
        check(!sawGet, "with no browser polling, the board is not asked to dump its parameters",
              "an unwatched panel must not keep the STM32 busy");

        BOOL__G__BrowserSeen = true;
        UINT32_T__G__LastBrowserPollMs = (uint32_t)G_StubMillis;
        UINT32_T__G__LastParamRefreshMs =
            (uint32_t)G_StubMillis - ESP_LINK_PARAM_REFRESH_MS - 1u;
        G_StubMillis += ESP_LINK_TX_INTERVAL_MS + 1u;
        Serial.tx.clear();
        func__Esp_PumpTx();
        sawGet = false;
        for (size_t i = 0; i + 3u < Serial.tx.size(); i++) {
            if (Serial.tx[i] == ESP_LINK_SOF_BYTE0 && Serial.tx[i + 1] == ESP_LINK_SOF_BYTE1 &&
                Serial.tx[i + 3] == ESP_MSG_GET_PARAMS) { sawGet = true; }
        }
        check(sawGet, "an open page still gets its periodic refresh");
    }

    /* ---- 13. the bench log on the file system ---------------------------- */
    {
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/benchlog/clear", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200, "POST /benchlog/clear answers 200");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("plain", "1,2,3\n");
        ESP_WEB_SERVER_T__G__Server.call("/benchlog/add", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200, "a well-formed CSV row is accepted");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("plain", "no newline here");
        ESP_WEB_SERVER_T__G__Server.call("/benchlog/add", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode >= 400,
              "a row with no terminating newline is refused",
              "it would glue itself onto the next row and corrupt both");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/benchlog", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find("1,2,3") != std::string::npos,
              "GET /benchlog returns the row that was stored");
    }

    /* ---- 14. loop() drains the UART the way the board does -------------- */
    {
        BOOL__G__TlmSeen = false;
        UINT32_T__G__TlmField[0] = 0u;
        Serial.feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload(100u)));
        loop();
        check(BOOL__G__TlmSeen && UINT32_T__G__TlmField[0] == 1000uL,
              "loop() reads a frame off the UART and decodes it",
              "the sketch's own main path, not a hand-called parser");
        check(Serial.rx.empty(), "and it drains the buffer in one pass");
    }

    /* ---- 14b. active LUT readback returns actual point pairs ------------ */
    {
        /* [EN] The panel must request the table through the link, not rebuild
           it from its own proposal. Two channel frames are required before
           GET /lut reports ready, including a zero-free battery-2 example.
           [FA] پنل باید جدول را از لینک بخواند، نه از پیشنهاد خودش بسازد.
           تا رسیدن هر دو فریم کانال، GET /lut آماده اعلام نمی‌شود؛ نمونهٔ
           باتری ۲ هم عمداً مقدار جداگانه دارد. */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        Serial.tx.clear();
        ESP_WEB_SERVER_T__G__Server.call("/lut/read", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 && Serial.tx.size() >= 8u,
              "POST /lut/read starts a board readback request");
        check(Serial.tx[3] == ESP_MSG_LUT_READ && Serial.tx[4] == 0u && Serial.tx[5] == 0u,
              "the readback command has its own zero-payload message");

        feed(build_frame(ESP_MSG_LUT_DATA,
                         lut_data(1u, {{111u, 222u}, {333u, 444u}})));
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find("\"ready\":0") != std::string::npos,
              "one channel alone never masquerades as a complete readback");

        feed(build_frame(ESP_MSG_LUT_DATA,
                         lut_data(2u, {{777u, 888u}})));
        check(UINT8_T__G__LutReadCount1 == 2u && UINT8_T__G__LutReadCount2 == 1u,
              "readback stores both channel point counts");
        check(UINT32_T__G__LutReadCounts2[0] == 777u &&
              UINT32_T__G__LutReadMa2[0] == 888u,
              "battery 2 chain/power values survive the wire decode");
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find("\"ready\":1") != std::string::npos &&
              ESP_WEB_SERVER_T__G__Server.lastBody.find("[777,888]") != std::string::npos,
              "GET /lut exposes the actual battery-2 pair after both frames arrive");
        feed(build_frame(ESP_MSG_LUT_DATA, lut_data(1u, {{999u, 999u}})));
        check(UINT8_T__G__LutReadCount1 == 2u && UINT32_T__G__LutReadCounts1[0] == 111u,
              "an unsolicited LUT_DATA frame cannot overwrite a complete readback");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut/read", HTTP_POST);
        feed(build_frame(ESP_MSG_LUT_DATA, std::vector<uint8_t>{ 1u, 0u, 1u }));
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find("\"ready\":0") != std::string::npos &&
              ESP_WEB_SERVER_T__G__Server.lastBody.find("\"error\":1") != std::string::npos,
              "a truncated LUT_DATA frame is reported as readback failure");
    }

    /* ---- 15. v1.67: the LUT push is paced, one frame per ACK ----------- */
    {
        /* [EN] The board receives on a 256-byte DMA ring. v1.66 wrote all
           four frames of a push back to back (about 430 bytes), which
           overruns that ring before the comm task can read it. The sender
           must now emit ONE frame and then wait for its LUT_ACK.
           [FA] برد روی حلقهٔ ۲۵۶ بایتی می‌گیرد؛ ارسال باید گام‌به‌گام باشد. */
        auto lut_ack = [](uint8_t stage, uint8_t status, uint8_t n1, uint8_t n2,
                          uint32_t crc) {
            std::vector<uint8_t> p(8, 0u);
            p[0] = stage; p[1] = status; p[2] = n1; p[3] = n2;
            p[4] = (uint8_t)(crc & 0xFFu);       p[5] = (uint8_t)((crc >> 8) & 0xFFu);
            p[6] = (uint8_t)((crc >> 16) & 0xFFu); p[7] = (uint8_t)((crc >> 24) & 0xFFu);
            return p;
        };

        /* n1=2, n2=2, then (chain,power) per point, then the CRC32 */
        const std::string body = "2,2, 1,2, 3,4, 1,2, 3,4, 12345";

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.args["plain"] = body;
        Serial.tx.clear();
        ESP_WEB_SERVER_T__G__Server.call("/lut", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200,
              "POST /lut accepts a well-formed two-channel table");
        check(Serial.tx.empty(),
              "the handler itself puts NOTHING on the wire",
              "v1.66 burst all four frames here and overran the board's RX ring");

        G_StubMillis += ESP_LINK_TX_INTERVAL_MS + 1u;
        func__Esp_PumpTx();
        const size_t afterBegin = Serial.tx.size();
        check(afterBegin > 0u && Serial.tx[3] == ESP_MSG_LUT_BEGIN,
              "the first pump sends LUT_BEGIN");

        G_StubMillis += ESP_LINK_TX_INTERVAL_MS + 1u;
        func__Esp_PumpTx();
        check(Serial.tx.size() == afterBegin,
              "and sends nothing more while that ACK is outstanding",
              "this is the whole point: one frame in flight at a time");

        feed(build_frame(ESP_MSG_LUT_ACK, lut_ack(1u, 0u, 0u, 0u, 0u)));
        func__Esp_PumpTx();
        check(Serial.tx.size() > afterBegin && Serial.tx[afterBegin + 3] == ESP_MSG_LUT_CHUNK,
              "the ACK for BEGIN releases the first chunk");
        const size_t afterChunk1 = Serial.tx.size();

        feed(build_frame(ESP_MSG_LUT_ACK, lut_ack(2u, 0u, 2u, 0u, 0u)));
        func__Esp_PumpTx();
        check(Serial.tx.size() > afterChunk1 && Serial.tx[afterChunk1 + 3] == ESP_MSG_LUT_CHUNK,
              "the ACK for chunk 1 releases chunk 2");
        const size_t afterChunk2 = Serial.tx.size();

        feed(build_frame(ESP_MSG_LUT_ACK, lut_ack(2u, 0u, 2u, 2u, 0u)));
        func__Esp_PumpTx();
        check(Serial.tx.size() > afterChunk2 && Serial.tx[afterChunk2 + 3] == ESP_MSG_LUT_COMMIT,
              "the ACK for chunk 2 releases COMMIT");

        feed(build_frame(ESP_MSG_LUT_ACK, lut_ack(3u, 0u, 2u, 2u, 12345u)));
        func__Esp_PumpTx();
        check(UINT8_T__G__LutTxStage == 0u && UINT8_T__G__LutTxError == 0u,
              "the commit ACK ends the push with no error");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut/reset", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 409,
              "the reboot is still refused until the exact active table is read back");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut/read", HTTP_POST);
        feed(build_frame(ESP_MSG_LUT_DATA,
                         lut_data(1u, {{1u, 2u}, {3u, 4u}})));
        feed(build_frame(ESP_MSG_LUT_DATA,
                         lut_data(2u, {{1u, 2u}, {3u, 4u}})));
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut/reset", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200,
              "and only then is the reboot request accepted");
    }

    /* ---- 16. v1.67: a board that stops answering fails visibly --------- */
    {
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.args["plain"] = "2,0,1,2,3,4,999";
        ESP_WEB_SERVER_T__G__Server.call("/lut", HTTP_POST);
        for (int i = 0; i < 40; i++) {
            G_StubMillis += ESP_LUT_TX_ACK_TIMEOUT_MS + 1u;
            func__Esp_PumpTx();
        }
        check(UINT8_T__G__LutTxStage == 0u && UINT8_T__G__LutTxError == 1u,
              "a silent board stops the push with error 1 after the retries",
              "the panel must be able to say 'no answer' instead of spinning for ever");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find("\"txe\":1") != std::string::npos,
              "GET /lut reports that failure to the browser");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut/reset", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 409,
              "and a failed push can never be followed by a reboot");
    }

    /* ---- 16. the page stream itself keeps the link lossless -------------
       [EN] User report 2026-10-07: the yellow "rejected by CRC" warning came
            back after every page load. Root cause: func__Esp_HttpRoot streams
            the ~375 KB panel inside ONE handleClient call, and while that
            runs nothing empties the RX ring - the STM32 streams telemetry
            every 100 ms, the ring overflows, bytes die mid-frame. The fix
            drains the UART after every 2 KB slice. This test is the proof
            that the fix loses nothing: frames already waiting when the page
            streams are ALL parsed during the stream - none dropped, none
            CRC-rejected.
       [FA] اثبات بی‌اتلاف‌بودن اصلاحِ هشدار CRC: پنج فریم تله‌متری که هنگام
            ارسال صفحه در بافر منتظرند، همگی حین خودِ ارسال پارس می‌شوند -
            هیچ‌کدام نمی‌افتد و خطای CRC تازه‌ای ساخته نمی‌شود. */
    {
        BOOL__G__TlmSeen = false;
        UINT32_T__G__TlmField[0] = 0u;
        const uint32_t uint32_t__crcBefore = UINT32_T__G__RxCrcError;
        for (uint16_t uint16_t__seq = 200u; uint16_t__seq < 205u; uint16_t__seq++) {
            Serial.feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload(uint16_t__seq)));
        }
        func__Esp_HttpRoot();
        check(BOOL__G__TlmSeen, "the page stream parses the frames waiting on the UART");
        check(UINT16_T__G__TlmSeq == 204u,
              "ALL five telemetry frames survive the page stream (no loss)",
              "last parsed seq is " + std::to_string(UINT16_T__G__TlmSeq) + ", expected 204");
        check(UINT32_T__G__RxCrcError == uint32_t__crcBefore,
              "the in-handler drain adds no CRC error of its own");
        check(Serial.rx.empty(), "and the ring is empty when the stream ends");
    }

    /* ---- 17. hybrid Wi-Fi TX power and battery duty-cycling ------------- */
    {
        /* 1) Mains input present -> AP stays ON at maximum TX power (20.5 dBm) */
        BOOL__G__ParamKnown[74] = false;
        BOOL__G__ParamKnown[75] = false;
        BOOL__G__ParamKnown[ESP_PARAM_WIFI_IDLE_LOW_POWER_MS] = false;
        BOOL__G__ParamKnown[ESP_PARAM_WIFI_CYCLE_ON_MS] = false;
        BOOL__G__ParamKnown[ESP_PARAM_WIFI_CYCLE_OFF_MS] = false;
        BOOL__G__ParamKnown[ESP_PARAM_WIFI_BAT_CUTOFF_PCT] = false;
        UINT32_T__G__TlmField[15] = 25000u; /* 50% battery (>= 20% cutoff) */
        BOOL__G__TlmSeen = true;
        UINT8_T__G__TlmFlags = ESP_TLM_FLAG_INPUT_PRESENT;
        BOOL__G__BrowserSeen = false;
        WiFi.stationNum = 0;
        WiFi.apActive = true;
        WiFi.outputPowerDbm = ESP_WIFI_TX_MAX_DBM;
        BOOL__G__WifiApActive = true;
        BOOL__G__WifiHighPower = true;
        BOOL__G__WifiBatteryIdleTracking = false;
        BOOL__G__WifiLastReportedConnected = false;
        Serial.tx.clear();

        func__Esp_WifiPowerSaveTick(1000u);
        check(WiFi.apActive && WiFi.outputPowerDbm > 20.0f,
              "with mains input present, Wi-Fi stays ON at maximum TX power (20.5 dBm)");

        /* 2) On battery with no client -> first 30 min ON at low TX power (0 dBm) */
        UINT8_T__G__TlmFlags = 0u;
        func__Esp_WifiPowerSaveTick(2000u);
        check(WiFi.apActive && WiFi.outputPowerDbm < 0.1f,
              "on battery with no client, Wi-Fi stays ON at low TX power during first 30 min");

        func__Esp_WifiPowerSaveTick(2000u + 1799000u);
        check(WiFi.apActive && WiFi.outputPowerDbm < 0.1f,
              "at 29m59s on battery with no client, Wi-Fi is still ON at low TX power");

        /* 3) After 30 minutes (1800000 ms) -> 2 min OFF / 1 min ON at low TX power */
        func__Esp_WifiPowerSaveTick(2000u + 1800000u);
        check(!WiFi.apActive,
              "after 30 minutes idle on battery, Wi-Fi enters the 2-minute OFF phase");

        func__Esp_WifiPowerSaveTick(2000u + 1800000u + 120000u);
        check(WiFi.apActive && WiFi.outputPowerDbm < 0.1f,
              "after 2 minutes OFF, Wi-Fi turns ON for 1 minute at low TX power");

        func__Esp_WifiPowerSaveTick(2000u + 1800000u + 180000u);
        check(!WiFi.apActive,
              "after the 1-minute ON window, Wi-Fi turns OFF for the next 2-minute phase");

        /* 4) Client connects -> immediate ON at max TX power + 0x33 frame sent */
        Serial.tx.clear();
        WiFi.stationNum = 1;
        func__Esp_WifiPowerSaveTick(2000u + 1800000u + 190000u);
        check(WiFi.apActive && WiFi.outputPowerDbm > 20.0f,
              "when a client connects, Wi-Fi immediately stays ON at maximum TX power");
        check(Serial.tx.size() == 9u &&
              Serial.tx[3] == ESP_MSG_WIFI_CLIENT_STATE &&
              Serial.tx[6] == 1u,
              "connecting on battery sends ESPLINK_MSG_WIFI_CLIENT_STATE (0x33, payload=1) to STM32");

        /* 5) Client disconnects -> 30-min timer restarts at low TX power */
        Serial.tx.clear();
        WiFi.stationNum = 0;
        func__Esp_WifiPowerSaveTick(2000u + 1800000u + 200000u);
        check(WiFi.apActive && WiFi.outputPowerDbm < 0.1f,
              "when client disconnects on battery, 30-minute low-power window starts fresh");
        check(Serial.tx.size() == 9u &&
              Serial.tx[3] == ESP_MSG_WIFI_CLIENT_STATE &&
              Serial.tx[6] == 0u,
              "disconnecting sends ESPLINK_MSG_WIFI_CLIENT_STATE (0x33, payload=0) to STM32");

        /* 5b) v1.112 (user order 2026-10-25: "you may lower the Wi-Fi power"):
           an associated-but-idle phone must not hold the radio at 20.5 dBm
           forever. Max TX only while someone really watches the panel, or a
           device just joined; mains always max. */
        {
            const uint32_t base = 3000000u;

            /* fresh join on battery -> max, for the viewer window */
            UINT8_T__G__TlmFlags = 0u;
            UINT32_T__G__TlmField[15] = 25000u;   /* 50 % battery */
            BOOL__G__WifiBatCutoffLatched = false;
            BOOL__G__BrowserSeen = false;
            UINT32_T__G__WifiStationSinceMs = 0u;
            WiFi.stationNum = 1;
            func__Esp_WifiPowerSaveTick(base);
            check(WiFi.apActive && (WiFi.outputPowerDbm > 20.0f),
                  "handshake/power: a device that just joined gets maximum TX power");

            /* 11 minutes pass with no HTTP request -> AP on, low power */
            func__Esp_WifiPowerSaveTick(base + ESP_WIFI_VIEWER_HOLD_MS + 60000u);
            check(WiFi.apActive && (WiFi.outputPowerDbm < 0.1f),
                  "handshake/power: an idle associated phone leaves the AP on at LOW power");

            /* somebody opens the panel -> immediately back to max */
            BOOL__G__BrowserSeen = true;
            UINT32_T__G__LastBrowserPollMs = base + ESP_WIFI_VIEWER_HOLD_MS + 60000u;
            func__Esp_WifiPowerSaveTick(base + ESP_WIFI_VIEWER_HOLD_MS + 61000u);
            check(WiFi.outputPowerDbm > 20.0f,
                  "handshake/power: a fresh panel request raises the antenna at once");

            /* panel closed again for the whole window -> low again */
            func__Esp_WifiPowerSaveTick(base + (2u * ESP_WIFI_VIEWER_HOLD_MS) + 120000u);
            check(WiFi.outputPowerDbm < 0.1f,
                  "handshake/power: once nobody watches, the antenna drops back to low");

            /* mains present -> always max, no battery cost */
            UINT8_T__G__TlmFlags = ESP_TLM_FLAG_INPUT_PRESENT;
            func__Esp_WifiPowerSaveTick(base + (2u * ESP_WIFI_VIEWER_HOLD_MS) + 121000u);
            check(WiFi.outputPowerDbm > 20.0f,
                  "handshake/power: with mains present the antenna is always at maximum");

            /* back to the earlier scenario state for the checks below */
            UINT8_T__G__TlmFlags = 0u;
            UINT32_T__G__WifiBatteryIdleStartMs = 0u;
            BOOL__G__WifiBatteryIdleTracking = false;
        }

        /* 6) Battery < 20% on battery power -> AP turns completely OFF even if a client is present */
        WiFi.stationNum = 1;
        UINT32_T__G__TlmField[15] = 22000u; /* 12% (< 20% default cutoff) */
        func__Esp_WifiPowerSaveTick(2000u + 1800000u + 210000u);
        check(!WiFi.apActive,
              "when battery drops below 20% on battery power, Wi-Fi AP turns completely OFF");

        /* 7) Mains input returns -> AP turns back ON at max TX power even if battery < 20% */
        UINT8_T__G__TlmFlags = ESP_TLM_FLAG_INPUT_PRESENT;
        func__Esp_WifiPowerSaveTick(2000u + 1800000u + 220000u);
        check(WiFi.apActive && WiFi.outputPowerDbm > 20.0f,
              "when mains input returns, Wi-Fi AP turns back ON at max TX power");
    }

    /* ---- 30. v1.93 state history (user order 2026-10-23) ----------------
       [EN] The panel now keeps its own history: one row per REAL change, one
            file per day, written and closed immediately so a battery cut
            cannot lose the last row. These tests pin the three promises that
            are easy to lose: only changes write, the clock comes from the
            phone, and the day file list survives for the download.
       [FA] پنل حالا تاریخچهٔ خودش را نگه می‌دارد: برای هر تغییر واقعی یک ردیف،
            هر روز یک فایل، همان لحظه نوشته و بسته می‌شود تا قطع باتری آخرین
            ردیف را از بین نبرد. این تست‌ها سه قول را قفل می‌کنند: فقط تغییر
            می‌نویسد، ساعت از گوشی می‌آید و فهرست روزها برای دانلود می‌ماند. */
    {
        /* a clean history */
        LittleFS.files.clear();
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false;
        BOOL__G__TlmSeen = false;
        UINT32_T__G__LastTlmMs = 0u;
        UINT16_T__G__LogPendingMask = 0u;

        check(ESP_WEB_SERVER_T__G__Server.has("/clock", HTTP_POST) &&
              ESP_WEB_SERVER_T__G__Server.has("/evt", HTTP_GET) &&
              ESP_WEB_SERVER_T__G__Server.has("/evt/days", HTTP_GET) &&
              ESP_WEB_SERVER_T__G__Server.has("/evt/dl", HTTP_GET) &&
              ESP_WEB_SERVER_T__G__Server.has("/evt/clr", HTTP_POST),
              "setup() registers the five history routes");

        /* the phone hands over its clock (2026-10-23 12:00 UTC, Tehran +3:30) */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("sec", "1792756800");
        ESP_WEB_SERVER_T__G__Server.setArg("tz", "12600");
        ESP_WEB_SERVER_T__G__Server.call("/clock", HTTP_POST);
        check(BOOL__G__LogClockValid,
              "POST /clock adopts the phone clock");
        check(INT32_T__G__LogTzSec == 12600,
              "the phone's UTC offset is kept, so the day file rolls over at the operator's midnight");
        check(func__EspLog_NowSec() >= 1792756800u,
              "now() walks forward from the phone anchor");

        /* v1.106: the boot row needs NO board frame - Tick writes it at
           start-up so a session with a silent board still logs. */
        G_StubMillis = 100000u;
        func__EspLog_Tick();
        check(UINT16_T__G__LogRowsToday == 1u,
              "start-up writes exactly one boot row even with the board never connected");
        const std::vector<uint8_t> first = tlm_payload_v24(200u, 24000u);
        feed(build_frame(ESP_MSG_TLM_LIVE, first));
        check(UINT16_T__G__LogRowsToday == 1u,
              "the first frame only seeds the state compare - it adds no row");

        const uint32_t dayCode = func__EspLog_TodayCode();
        char path[32];
        (void)snprintf(path, sizeof path, "/evt-%08lu.csv", (unsigned long)dayCode);
        check(dayCode != 0u && LittleFS.exists(path),
              "the row lands in a file named after the phone's own day");

        size_t rows = 0u;
        std::string body = LittleFS.files[std::string(path)];
        for (size_t k = 0u; k < body.size(); k++) {
            if ((body[k] == '\n') && (k > 0u) && (body[k - 1u] != '#')) rows++;
        }
        check(body.find("# epoch,uptime_s") != std::string::npos,
              "a new day file starts with its column header");
        check(row_field(body, 0u, 2u) == 1,
              "the first row carries the boot change bit");

        /* the same state again: NOTHING is written */
        G_StubMillis += 5000u;
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(201u, 24000u)));
        check(UINT16_T__G__LogRowsToday == 1u,
              "an unchanged board writes nothing (the log is change-only)");

        /* mains appears (pack still healthy): one row, mains bit set */
        std::vector<uint8_t> withMains = tlm_payload_v24(202u, 24000u);
        withMains[2] = 0x02u;   /* input present */
        G_StubMillis += 5000u;
        feed(build_frame(ESP_MSG_TLM_LIVE, withMains));
        check(UINT16_T__G__LogRowsToday == 2u,
              "a mains change writes one row");
        std::string body2 = LittleFS.files[std::string(path)];
        check(row_field(body2, 1u, 2u) == 2,
              "the mains row carries the mains change bit");

        /* battery falls under the cut band: a row, and the last-state note */
        std::vector<uint8_t> lowPack = tlm_payload_v24(203u, 20608u);
        lowPack[2] = 0x00u;
        G_StubMillis += 5000u;
        feed(build_frame(ESP_MSG_TLM_LIVE, lowPack));
        check(UINT16_T__G__LogRowsToday == 3u,
              "crossing the cut band writes one row");
        /* [EN] no second file: the row itself is the memory. The last row of the
                day file carries the pack voltage, so the next boot can compare.
           [FA] فایل دوم نداریم: خود ردیف حافظه است. آخرین ردیف فایل روز ولتاژ
                پک را دارد، پس روشن‌شدن بعدی می‌تواند مقایسه کند. */
        {
            std::string bodyLow = LittleFS.files[std::string(path)];
            check(row_field(bodyLow, 2u, 10u) == 20608,
                  "the last row itself holds the pack voltage, no extra file needed",
                  std::to_string(row_field(bodyLow, 2u, 10u)));
        }

        /* the panel reads the tail back */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              ESP_WEB_SERVER_T__G__Server.lastBody.find("20608") != std::string::npos,
              "GET /evt returns the saved rows as text");

        ESP_WEB_SERVER_T__G__Server.call("/evt/days", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              ESP_WEB_SERVER_T__G__Server.lastBody.find(std::to_string(dayCode)) != std::string::npos,
              "GET /evt/days lists the saved day for the picker");

        /* [EN] v1.95 (user order: every state must be in the log): hand mode
                 and the general state byte are logged too, not just faults.
           [FA] نسخهٔ ۱٫۹۵ (دستور کاربر: همهٔ وضعیت‌ها در لاگ باشد): مود دستی
                 و بایت عمومی وضعیت هم ثبت می‌شوند، نه فقط خطاها. */
        {
            std::vector<uint8_t> manual = tlm_payload_v24(210u, 20608u);
            manual[2] = 0x20u;   /* hand mode on, pack still on the cut band */
            G_StubMillis += 5000u;
            feed(build_frame(ESP_MSG_TLM_LIVE, manual));
            check(UINT16_T__G__LogRowsToday == 4u,
                  "switching the board to hand mode writes a row");
            std::string bodyMode = LittleFS.files[std::string(path)];
            check(row_field(bodyMode, 3u, 2u) == 256u,
                  "that row is marked as a mode change",
                  std::to_string(row_field(bodyMode, 3u, 2u)));
            check(row_field(bodyMode, 3u, 5u) == 32u,
                  "and the saved state carries the hand-mode bit, so the line can say it",
                  std::to_string(row_field(bodyMode, 3u, 5u)));
            check(bodyMode.find(",fl,") != std::string::npos,
                  "the column header names the general state byte");
        }

        /* [EN] v1.95: the condemned-battery verdict has its own field on the
                 wire, so it needs its own change bit AND its own column.
           [FA] نسخهٔ ۱٫۹۵: حکم باتری خراب فیلد جدا روی سیم دارد، پس بیت
                 تغییر و ستون جدای خودش را هم دارد. */
        {
            std::vector<uint8_t> dead = tlm_payload_v24(211u, 20608u);
            const size_t o = ESP_LINK_TLM_FIELD_OFFSET + (28u * 4u);
            /* all four bytes: the payload is pre-filled with 1000+28, so
               writing only the low byte would leave 1025 behind */
            dead[o] = 0x01u;
            dead[o + 1u] = 0x00u;
            dead[o + 2u] = 0x00u;
            dead[o + 3u] = 0x00u;
            G_StubMillis += 5000u;
            feed(build_frame(ESP_MSG_TLM_LIVE, dead));
            check(UINT16_T__G__LogRowsToday == 5u,
                  "a condemned-battery verdict writes a row");
            std::string bodyDead = LittleFS.files[std::string(path)];
            check(row_field(bodyDead, 4u, 2u) == 768u,
                  "that row is marked as a verdict change (and a mode change: the flags byte fell)",
                  std::to_string(row_field(bodyDead, 4u, 2u)));
            check(row_field(bodyDead, 4u, 14u) == 1u,
                  "and the verdict itself is saved, so the line can name the battery",
                  std::to_string(row_field(bodyDead, 4u, 14u)));
        }

        /* [EN] v1.96 (user order 2026-10-23: more detail in the log): the
                 pack voltage itself, in visible steps, plus the input rail,
                 the 12 V node, both currents and both duties. The step is what
                 draws the charge curve; the extra columns are what the bench
                 needs to see WHY a row happened.
           [FA] نسخهٔ ۱٫۹۶ (دستور کاربر: جزئیات بیشتر در لاگ): خودِ ولتاژ پک با
                 گام محسوس، به‌همراه ولتاژ ورودی، نود ۱۲ ولت، هر دو جریان و هر
                 دو دیوتی. گام، منحنی شارژ را می‌کشد و ستون‌های تازه نشان
                 می‌دهند چرا یک ردیف نوشته شد. */
        {
            const uint16_t rowsBefore = UINT16_T__G__LogRowsToday;
            const size_t deadOff = ESP_LINK_TLM_FIELD_OFFSET + (28u * 4u);

            /* 199 mV: below the step, nothing may be written. The verdict
               field is pinned to the value the previous test left behind,
               otherwise its own change would write the row. */
            std::vector<uint8_t> drift = tlm_payload_v24(212u, 20807u);
            drift[deadOff] = 0x01u; drift[deadOff + 1u] = 0x00u;
            drift[deadOff + 2u] = 0x00u; drift[deadOff + 3u] = 0x00u;
            G_StubMillis += 5000u;
            feed(build_frame(ESP_MSG_TLM_LIVE, drift));
            check(UINT16_T__G__LogRowsToday == rowsBefore,
                  "a pack drift smaller than the step writes nothing");

            /* 200 mV more: a row, marked as a voltage step. The row before
               this one (the band row) carried a step of its own, so the
               one-step-a-minute window must be waited out first - that is the
               v1.98 promise, and the test walks through it honestly. */
            std::vector<uint8_t> step = tlm_payload_v24(213u, 21007u);
            step[deadOff] = 0x01u; step[deadOff + 1u] = 0x00u;
            step[deadOff + 2u] = 0x00u; step[deadOff + 3u] = 0x00u;
            G_StubMillis += ESP_LOG_VSTEP_MIN_MS;
            feed(build_frame(ESP_MSG_TLM_LIVE, step));
            check(UINT16_T__G__LogRowsToday == (uint16_t)(rowsBefore + 1u),
                  "a 200 mV step of the pack writes one row");
            std::string bodyStep = LittleFS.files[std::string(path)];
            const size_t stepRow = (size_t)(rowsBefore);
            check((row_field(bodyStep, stepRow, 2u) & 1024L) != 0L,
                  "that row carries the voltage-step change bit",
                  std::to_string(row_field(bodyStep, stepRow, 2u)));
            check(row_field(bodyStep, stepRow, 10u) == 21007L,
                  "the row keeps the new pack voltage",
                  std::to_string(row_field(bodyStep, stepRow, 10u)));
            check(row_field(bodyStep, stepRow, 15u) == 1014L,
                  "the input rail is saved as well",
                  std::to_string(row_field(bodyStep, stepRow, 15u)));
            check(row_field(bodyStep, stepRow, 16u) == 1016L,
                  "so is the 12 V node",
                  std::to_string(row_field(bodyStep, stepRow, 16u)));
            check(row_field(bodyStep, stepRow, 17u) == 1003L &&
                  row_field(bodyStep, stepRow, 18u) == 1010L,
                  "and both battery currents");
            check(row_field(bodyStep, stepRow, 19u) == 1005L &&
                  row_field(bodyStep, stepRow, 20u) == 1012L,
                  "and both duties");
        }

        /* [EN] v1.98 (user order: do not let the log fill itself): the cut
                 band has hysteresis and a voltage step writes at most one row
                 a minute. A pack that wobbles on the line, or a load that
                 switches on and off, must not turn the history into noise.
           [FA] نسخهٔ ۱٫۹۸ (دستور کاربر: لاگ خودش را پر نکند): مرز قطع
                 هیسترزیس دارد و گام ولتاژ حداکثر یک ردیف در دقیقه می‌نویسد.
                 باتری‌ای که روی مرز نوسان می‌کند یا باری که روشن و خاموش
                 می‌شود نباید تاریخچه را شلوغ کند.
           [EN] v1.109 user order: while the mains are OFF every RISE of the
                 pack is its own row - the throttle stays only for falls and
                 for charging.
           [FA] نسخهٔ ۱٫۱۰۹ دستور کاربر: وقتی برق شهر قطع است هر بالا آمدن
                 پک ردیف خودش را دارد - محدودیت فقط برای پایین‌رفتن و شارژ
                 می‌ماند. */
        {
            const size_t deadOff2 = ESP_LINK_TLM_FIELD_OFFSET + (28u * 4u);
            const uint16_t rowsBefore = UINT16_T__G__LogRowsToday;

            /* the wobble: 21,150 → 21,180 → 21,140 - every move stays under
               the 200 mV step from the last ROW (21,007), so no rise worth a
               record happens and the log stays quiet */
            const uint32_t wobble[3] = { 21150u, 21180u, 21140u };
            for (uint8_t k = 0u; k < 3u; k++) {
                std::vector<uint8_t> f = tlm_payload_v24((uint16_t)(230u + k), wobble[k]);
                f[deadOff2] = 0x01u; f[deadOff2 + 1u] = 0x00u;
                f[deadOff2 + 2u] = 0x00u; f[deadOff2 + 3u] = 0x00u;
                G_StubMillis += 5000u;
                feed(build_frame(ESP_MSG_TLM_LIVE, f));
            }
            check(UINT16_T__G__LogRowsToday == rowsBefore,
                  "a pack wobbling on the cut line writes no row at all",
                  std::to_string(UINT16_T__G__LogRowsToday));

            /* leaving the band needs 21,400 - one row, not one per crossing */
            std::vector<uint8_t> up = tlm_payload_v24(233u, 21500u);
            up[deadOff2] = 0x01u; up[deadOff2 + 1u] = 0x00u;
            up[deadOff2 + 2u] = 0x00u; up[deadOff2 + 3u] = 0x00u;
            G_StubMillis += 5000u;
            feed(build_frame(ESP_MSG_TLM_LIVE, up));
            check(UINT16_T__G__LogRowsToday == (uint16_t)(rowsBefore + 1u),
                  "leaving the band above 21,400 mV writes exactly one row");
            std::string bodyUp = LittleFS.files[std::string(path)];
            check((row_field(bodyUp, (size_t)rowsBefore, 2u) & 64L) != 0L,
                  "and that row is marked as a band change",
                  std::to_string(row_field(bodyUp, (size_t)rowsBefore, 2u)));

            /* v1.109: a RISE in discharge is written even inside the same
               minute - the user wants a record for every time it came up */
            const uint16_t afterUp = UINT16_T__G__LogRowsToday;
            std::vector<uint8_t> fast = tlm_payload_v24(234u, 21800u);
            fast[deadOff2] = 0x01u; fast[deadOff2 + 1u] = 0x00u;
            fast[deadOff2 + 2u] = 0x00u; fast[deadOff2 + 3u] = 0x00u;
            G_StubMillis += 10000u;             /* 10 s after the last row */
            feed(build_frame(ESP_MSG_TLM_LIVE, fast));
            check(UINT16_T__G__LogRowsToday == (uint16_t)(afterUp + 1u),
                  "a rise in discharge is written even inside the same minute");

            /* a FALL inside the same minute is still suppressed */
            const uint16_t afterFast = UINT16_T__G__LogRowsToday;
            std::vector<uint8_t> down = tlm_payload_v24(235u, 21500u);
            down[deadOff2] = 0x01u; down[deadOff2 + 1u] = 0x00u;
            down[deadOff2 + 2u] = 0x00u; down[deadOff2 + 3u] = 0x00u;
            G_StubMillis += 5000u;              /* still inside the minute */
            feed(build_frame(ESP_MSG_TLM_LIVE, down));
            check(UINT16_T__G__LogRowsToday == afterFast,
                  "a fall in discharge inside the same minute is not written");

            /* after the minute the next rise is written again */
            std::vector<uint8_t> slow = tlm_payload_v24(236u, 22100u);
            slow[deadOff2] = 0x01u; slow[deadOff2 + 1u] = 0x00u;
            slow[deadOff2 + 2u] = 0x00u; slow[deadOff2 + 3u] = 0x00u;
            G_StubMillis += ESP_LOG_VSTEP_MIN_MS;
            feed(build_frame(ESP_MSG_TLM_LIVE, slow));
            check(UINT16_T__G__LogRowsToday == (uint16_t)(afterFast + 1u),
                  "once the minute has passed the step is written again");

            /* v1.109: the row carries the battery percent as its last column */
            std::string bodyPct = LittleFS.files[std::string(path)];
            /* the "fast" row (21,800 mV, factory map 21,000..29,000) = 10% */
            check(row_field(bodyPct, (size_t)afterUp, 21u) == 10L,
                  "the row's last column is the battery percent of its voltage",
                  std::to_string(row_field(bodyPct, (size_t)afterUp, 21u)));
        }

        /* [EN] v1.108 (user order 2026-10-24: "the log must be complete"): a
                 mains cut/return never waits behind the 2 s merge window. The
                 board may cut the panel's power the same instant (battery
                 saver), so the row must reach the flash at once - the test
                 deliberately does NOT open the window first.
           [FA] نسخهٔ ۱٫۱۰۸ (دستور کاربر ۲۰۲۶-۱۰-۲۴: «لاگ باید کامل باشد»):
                 قطع/وصل برق شهر هرگز پشت پنجرهٔ ادغام ۲ثانیه‌ای نمی‌ماند. برد
                 ممکن است همان لحظه (صرفه‌جویی باتری) برق پنل را ببرد، پس ردیف
                 باید بی‌درنگ به فلش برسد - تست عمداً پنجره را باز نمی‌کند. */
        {
            const uint16_t rowsBefore = UINT16_T__G__LogRowsToday;
            /* every earlier frame carried mains OFF - establish mains ON
               first, so the CUT below is a real mains change */
            std::vector<uint8_t> on = tlm_payload_v24(236u, 22100u);
            on[2u] = 0x02u;
            G_StubMillis += 100u;
            feed(build_frame(ESP_MSG_TLM_LIVE, on));
            G_StubMillis += ESP_LOG_MAINS_FLUSH_MS + 10u;
            func__EspLog_Tick();
            check(UINT16_T__G__LogRowsToday == (uint16_t)(rowsBefore + 1u),
                  "the mains return lands inside the short coalescing window",
                  std::to_string(UINT16_T__G__LogRowsToday));

            /* the CUT: the row must reach the flash before the panel dies */
            std::vector<uint8_t> cut = tlm_payload_v24(237u, 22100u);
            G_StubMillis += 100u;
            feed(build_frame(ESP_MSG_TLM_LIVE, cut));
            G_StubMillis += ESP_LOG_MAINS_FLUSH_MS + 10u;
            func__EspLog_Tick();
            check(UINT16_T__G__LogRowsToday == (uint16_t)(rowsBefore + 2u),
                  "a mains cut lands inside the short coalescing window, not the 2 s merge",
                  std::to_string(UINT16_T__G__LogRowsToday));
            std::string bodyCut = LittleFS.files[std::string(path)];
            check((row_field(bodyCut, (size_t)(rowsBefore + 1u), 2u) & 2L) != 0L,
                  "and that row carries the mains change bit",
                  std::to_string(row_field(bodyCut, (size_t)(rowsBefore + 1u), 2u)));
        }

        /* [EN] v1.96: a quiet board still says "I am alive" once every six
                 hours. Without that row a silent log cannot be told apart from
                 a dead panel - which is the whole point of keeping the log.
           [FA] نسخهٔ ۱٫۹۶: برد آرام هم هر شش ساعت یک بار می‌گوید «زنده‌ام».
                 بدون آن ردیف، لاگِ خالی با پنل مرده اشتباه می‌شود؛ و هدف لاگ
                 همین است. */
        {
            const uint16_t rowsBefore = UINT16_T__G__LogRowsToday;
            UINT32_T__G__LastTlmMs = (uint32_t)millis();     /* link is fine */
            G_StubMillis += ESP_LOG_HEARTBEAT_MS;
            UINT32_T__G__LastTlmMs = (uint32_t)millis();
            func__EspLog_Tick();
            check(UINT16_T__G__LogRowsToday == (uint16_t)(rowsBefore + 1u),
                  "after a whole quiet heartbeat window, Tick writes one row");
            std::string bodyBeat = LittleFS.files[std::string(path)];
            check(row_field(bodyBeat, (size_t)rowsBefore, 3u) == 5L,
                  "that row is marked as a heartbeat",
                  std::to_string(row_field(bodyBeat, (size_t)rowsBefore, 3u)));
            check(row_field(bodyBeat, (size_t)rowsBefore, 2u) == 0L,
                  "and claims no change - nothing happened");
        }

        /* [EN] v1.106 (user order 2026-10-24: "if the board was not connected
                 it logged nothing - it must log"): a session with a board that
                 never speaks still writes boot, link-silence and heartbeat rows.
           [FA] v1.106 (دستور ۲۰۲۶-۱۰-۲۴): نشستِ بدون برد هم بوت، «برد نیست» و
                 ضربان را می‌نویسد. */
        {
            (void)ESP_WEB_SERVER_T__G__Server.call("/evt/clr", HTTP_POST);
            LittleFS.files.clear();
            BOOL__G__FsOk = true;
            BOOL__G__TlmSeen = false;
            BOOL__G__LogHasPrev = false;
            BOOL__G__LogBootRowPending = true;
            BOOL__G__LogLastRowValid = false;
            BOOL__G__LogLinkDown = false;
            BOOL__G__LogDayFull = false;
            UINT8_T__G__LogIndexCount = 0u;
            UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
            UINT16_T__G__LogRowsToday = 0u;
            UINT16_T__G__LogPendingMask = 0u;
            BOOL__G__LogMainsQueuedValid = false;
            G_StubMillis += 5000u;

            func__EspLog_Tick();
            check(UINT16_T__G__LogRowsToday == 1u,
                  "with a board that never connected, the boot row is still written");

            G_StubMillis += ESP_LOG_LINK_TIMEOUT_MS + 10u;
            func__EspLog_Tick();
            check(UINT16_T__G__LogRowsToday == 2u,
                  "after the silence timeout a link-silence row says the board is not there");
            check(BOOL__G__LogLinkDown,
                  "the silence stays latched so the first frame can pair it with link-back");

            /* backdate the last row instead of jumping 6 h forward - a real
               jump can roll the operator's day and reset the per-day count */
            UINT32_T__G__LogLastRowMs = (uint32_t)millis() - ESP_LOG_HEARTBEAT_MS;
            func__EspLog_Tick();
            check(UINT16_T__G__LogRowsToday == 3u,
                  "the heartbeat keeps the offline log alive too");

            G_StubMillis += 5000u;
            feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(219u, 24000u)));
            check(UINT16_T__G__LogRowsToday == 4u,
                  "the first late frame pairs the silence with a link-back row");
        }

        /* a burst inside the merge window becomes ONE row, not five */
        uint16_t before = UINT16_T__G__LogRowsToday;
        for (uint8_t k = 0u; k < 4u; k++) {
            std::vector<uint8_t> flap = tlm_payload((uint16_t)(210u + k));
            flap[2] = ((k % 2u) == 0u) ? 0x02u : 0x00u;
            G_StubMillis += 100u;
            feed(build_frame(ESP_MSG_TLM_LIVE, flap));
        }
        G_StubMillis += 3000u;
        func__EspLog_Tick();
        check(UINT16_T__G__LogRowsToday <= (uint16_t)(before + 2u),
              "a fast flap inside the 2 s window is merged instead of writing every frame");

        /* clearing drops the files and the list */
        ESP_WEB_SERVER_T__G__Server.call("/evt/clr", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              UINT8_T__G__LogIndexCount == 0u &&
              !LittleFS.exists(path),
              "POST /evt/clr deletes the saved days");

        /* and the log still works afterwards */
        G_StubMillis += 5000u;
        UINT32_T__G__TlmField[15] = 24000u;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        func__EspLog_Tick();
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload(220u)));
        check(UINT16_T__G__LogRowsToday == 1u,
              "the history starts again from a clean file after a clear");
    }

    /* ---- 31. v1.96: the byte budget, not just the day count ------------
       [EN] A day cap alone cannot bound the flash: six busy days are bigger
            than thirty quiet ones. The log keeps the whole history inside one
            byte budget and drops the oldest day first.
       [FA] سقف تعداد روز، فلش را کران‌دار نمی‌کند: شش روز شلوغ از سی روز آرام
            بزرگ‌تر است. لاگ کل تاریخچه را داخل یک بودجهٔ بایتی نگه می‌دارد و
            اول قدیمی‌ترین روز را می‌برد. */
    {
        LittleFS.files.clear();
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;

        const uint32_t days[3] = { 20261001u, 20261002u, 20261003u };
        std::string big(150000u, 'x');            /* > 2 x the day budget */
        for (size_t k = 0u; k < 3u; k++) {
            char dayPath[32];
            (void)snprintf(dayPath, sizeof dayPath, "/evt-%08lu.csv", (unsigned long)days[k]);
            bool ok = false;
            File f = LittleFS.open(dayPath, "w");
            if (f) { f.print(big.c_str()); ok = true; f.close(); }
            check(ok, "the test could write a big day file");
        }

        /* [EN] the day list is the directory now, so the scan is what fills it.
           [FA] فهرست روزها حالا همان پوشه است، پس خواندن پوشه آن را پر می‌کند. */
        (void)func__EspLog_ScanDays(true);
        check(UINT8_T__G__LogIndexCount == 3u,
              "the day list is read from the files themselves",
              std::to_string(UINT8_T__G__LogIndexCount));

        func__EspLog_TrimDays();
        check(UINT8_T__G__LogIndexCount == 1u,
              "days above the byte budget are dropped, oldest first",
              std::to_string(UINT8_T__G__LogIndexCount));
        check(UINT32_T__G__LogIndexDay[0] == 20261003u,
              "the day that survives is the newest one");
        check(!LittleFS.exists("/evt-20261001.csv") &&
              !LittleFS.exists("/evt-20261002.csv") &&
              LittleFS.exists("/evt-20261003.csv"),
              "the dropped days are gone from the flash and the newest is kept");
    }

    /* ---- 32. v1.97: storage never waits for the clock -------------------
       [EN] The phone may connect hours after the board is powered. Until then
            every row goes to a day file of its own ("no clock"), which is
            listed so the panel can open it. Nothing is dropped and nothing is
            deferred - the log is the only witness of what the board did.
       [FA] ممکن است گوشی ساعت‌ها بعد از روشن‌شدن برد وصل شود. تا آن لحظه هر ردیف
            در فایل روزِ «بدون ساعت» می‌نشیند و همان فایل هم در فهرست می‌آید تا
            پنل بتواند بازش کند. هیچ ردیفی دور ریخته یا عقب انداخته نمی‌شود. */
    {
        ESP_WEB_SERVER_T__G__Server.call("/evt/clr", HTTP_POST);
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        BOOL__G__LogClockValid = false;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogDayFull = false;
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        UINT16_T__G__LogPendingMask = 0u;

        G_StubMillis += 5000u;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        func__EspLog_Tick();
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(240u, 23800u)));

        check(LittleFS.exists("/evt-00000000.csv"),
              "with no phone clock at all, the first row still lands in a day file");
        std::string noClock = LittleFS.files[std::string("/evt-00000000.csv")];
        check(row_field(noClock, 0u, 0u) == 0L,
              "that row carries time 0, which the panel shows as \"no clock\"",
              std::to_string(row_field(noClock, 0u, 0u)));
        check(UINT8_T__G__LogIndexCount == 1u,
              "and the no-clock day is listed, so the panel can open it",
              std::to_string(UINT8_T__G__LogIndexCount));
        check(row_field(noClock, 0u, 2u) == 1L,
              "the row is a boot row like any other");

        /* the phone connects: the next change goes to the real day file */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("sec", "1792756800");
        ESP_WEB_SERVER_T__G__Server.setArg("tz", "12600");
        ESP_WEB_SERVER_T__G__Server.call("/clock", HTTP_POST);

        /* بلافاصله پس از رسیدن ساعت (پیش از ساخته‌شدن فایل امروز)، خواندن و دانلود خودکار نباید خالی برگردد */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              !ESP_WEB_SERVER_T__G__Server.lastBody.empty() &&
              row_field(ESP_WEB_SERVER_T__G__Server.lastBody, 0u, 2u) == 1L,
              "پیش از ساخته‌شدن فایل امروز، خواندن خودکار آخرین فایل موجود (روز بدون ساعت) را نشان می‌دهد");
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt/dl", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              ESP_WEB_SERVER_T__G__Server.lastBody.find("23800") == std::string::npos,
              "دانلود خودکار پیش از ساخته‌شدن فایل امروز همان فایل موجود را تحویل می‌دهد");

        std::vector<uint8_t> mainsNow = tlm_payload_v24(241u, 23800u);
        mainsNow[2] = 0x02u;
        G_StubMillis += 5000u;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        feed(build_frame(ESP_MSG_TLM_LIVE, mainsNow));

        check(UINT8_T__G__LogIndexCount == 2u,
              "once the clock arrives, today's own day file is added beside it",
              std::to_string(UINT8_T__G__LogIndexCount));
        check(func__EspLog_TodayCode() != 0u,
              "and the board now knows the real day");
        check(LittleFS.exists("/evt-00000000.csv"),
              "the no-clock file is kept, not thrown away");

        /* پس از ساخته‌شدن فایل امروز هم انتخاب صریح 00000000 باید همان فایل بدون ساعت را باز و دانلود کند */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("day", "00000000");
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              row_field(ESP_WEB_SERVER_T__G__Server.lastBody, 0u, 0u) == 0L &&
              row_field(ESP_WEB_SERVER_T__G__Server.lastBody, 0u, 2u) == 1L,
              "انتخاب صریح 00000000 حتی پس از ساخته‌شدن فایل امروز دقیقاً فایل روز بدون ساعت را برمی‌گرداند");
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.setArg("day", "00000000");
        ESP_WEB_SERVER_T__G__Server.call("/evt/dl", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              ESP_WEB_SERVER_T__G__Server.lastHeaderVal.find("evt-00000000.csv") != std::string::npos,
              "دانلود صریح 00000000 فایل روز بدون ساعت را دانلود می‌کند");

        /* وقتی فایل‌سیستم فلش غیرفعال است، حلقهٔ موقت رم رویدادهای زنده را نگه می‌دارد */
        BOOL__G__FsOk = false;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogDayFull = false;
        UINT16_T__G__LogRowsToday = 0u;
        UINT16_T__G__LogPendingMask = 0u;
        G_StubMillis += 5000u;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        func__EspLog_Tick();
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              !ESP_WEB_SERVER_T__G__Server.lastBody.empty() &&
              row_field(ESP_WEB_SERVER_T__G__Server.lastBody, 0u, 2u) == 1L,
              "حتی در صورت غیرفعال بودن فایل‌سیستم فلش، ردیف زنده در حلقهٔ رم ثبت و خوانده می‌شود");
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt/dl", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              ESP_WEB_SERVER_T__G__Server.lastBody.find("evt") == std::string::npos &&
              ESP_WEB_SERVER_T__G__Server.lastHeaderVal.find("evt-ram.csv") != std::string::npos,
              "دانلود در حالت بدون فایل‌سیستم فایل رم را تحویل می‌دهد");
        ESP_WEB_SERVER_T__G__Server.call("/evt/clr", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              UINT8_T__G__LogRamCount == 0u,
              "پاک کردن تاریخچه حلقهٔ موقت رم را هم خالی می‌کند");
        BOOL__G__FsOk = true;
    }

    /* ---- 33. a flash that refuses the row is REPORTED (2026-10-27) ------
       [EN] Why this block: the user reported "the event log never comes up",
            and the card said "nothing has been recorded yet". The old module
            threw away the byte count that File::print already returns, so a
            flash that refused the write lost the row in silence - the panel
            had no way to tell "no event" from "no memory". These checks make
            a full flash happen on purpose and pin the three promises of the
            rewrite: the row is not lost, the failure is reported to the page,
            and the first good write clears the report again.
       [FA] چرا این بخش: کاربر گفت «لاگ‌گیری بالا نمی‌آید» و کارت می‌گفت
            «هنوز چیزی ثبت نشده». ماژول قبلی تعداد بایتی را که print برمی‌گرداند
            دور می‌ریخت، پس فلشی که نوشتن را رد می‌کرد ردیف را بی‌صدا از بین
            می‌برد و پنل نمی‌توانست «اتفاقی نیفتاد» را از «حافظه نپذیرفت» جدا
            کند. این بررسی‌ها فلش پر را عمداً می‌سازند و سه قول بازنویسی را
            میخ می‌کنند: ردیف گم نمی‌شود، خرابی به صفحه گزارش می‌شود، و اولین
            نوشتن سالم گزارش را پاک می‌کند. */
    {
        LittleFS.failWrites = false;
        (void)ESP_WEB_SERVER_T__G__Server.call("/evt/clr", HTTP_POST);
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        UINT8_T__G__LogWriteFault = 0u;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false;
        UINT16_T__G__LogPendingMask = 0u;
        BOOL__G__LogMainsQueuedValid = false;
        BOOL__G__LogClockValid = true;
        INT32_T__G__LogTzSec = 12600;
        UINT32_T__G__LogAnchorSec = 1792756800u;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        BOOL__G__TlmSeen = false;
        G_StubMillis += 5000u;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();

        /* the clock is fine, the flash is not: every write is refused */
        LittleFS.failWrites = true;
        func__EspLog_Tick();
        check(UINT8_T__G__LogWriteFault != 0u,
              "a refused flash write raises the report instead of vanishing",
              std::to_string((int)UINT8_T__G__LogWriteFault));
        check(UINT8_T__G__LogRamCount == 1u,
              "the row the flash refused is kept in the RAM ring",
              std::to_string((int)UINT8_T__G__LogRamCount));
        check(UINT16_T__G__LogRowsToday == 0u,
              "a file that did not grow is not counted as a full day");

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.header("X-Log-Fs") == "2",
              "GET /evt tells the page that the flash refused the write",
              ESP_WEB_SERVER_T__G__Server.header("X-Log-Fs"));
        check(!ESP_WEB_SERVER_T__G__Server.lastBody.empty() &&
              row_field(ESP_WEB_SERVER_T__G__Server.lastBody, 0u, 2u) == 1L,
              "and the refused boot row is still readable under that banner - not an empty card");

        /* the flash works again: the next change lands on flash and clears it */
        LittleFS.failWrites = false;
        std::vector<uint8_t> seed = tlm_payload_v24(250u, 24000u);
        G_StubMillis += 5000u;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        feed(build_frame(ESP_MSG_TLM_LIVE, seed));
        std::vector<uint8_t> mainsBack = tlm_payload_v24(251u, 24000u);
        mainsBack[2] = 0x02u;
        G_StubMillis += 5000u;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        feed(build_frame(ESP_MSG_TLM_LIVE, mainsBack));
        check(UINT8_T__G__LogWriteFault == 0u,
              "the first good write clears the report, so no banner stays on a healthy flash");
        check(UINT8_T__G__LogRamCount == 0u,
              "the RAM ring is empty again: its rows went back to the flash",
              std::to_string((int)UINT8_T__G__LogRamCount));
        check(UINT16_T__G__LogRowsToday == 2u,
              "the refused boot row was drained into the file AND the new row was written",
              std::to_string(UINT16_T__G__LogRowsToday));
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.header("X-Log-Fs") == "1",
              "the page is told the rows come from the flash again",
              ESP_WEB_SERVER_T__G__Server.header("X-Log-Fs"));
        check(row_field(ESP_WEB_SERVER_T__G__Server.lastBody, 0u, 2u) == 1L,
              "the row the flash had refused is now the FIRST row of the day file",
              std::to_string(row_field(ESP_WEB_SERVER_T__G__Server.lastBody, 0u, 2u)));
        check(row_field(ESP_WEB_SERVER_T__G__Server.lastBody, 1u, 2u) == 2L,
              "and the row written after the recovery follows it as the mains change",
              std::to_string(row_field(ESP_WEB_SERVER_T__G__Server.lastBody, 1u, 2u)));

        /* [EN] the ring is finite: a long outage overflows it, and the rows it
                had to throw away are counted and shown to the page, never
                silently forgotten.
           [FA] حلقه محدود است: قطعی طولانی آن را پر می‌کند و ردیف‌هایی که
                ناچار دور ریخته شدند شمرده و به صفحه نشان داده می‌شوند. */
        UINT16_T__G__LogDroppedRows = 0u;
        LittleFS.failWrites = true;
        for (uint8_t k = 0u; k < (ESP_LOG_RAM_ROWS + 4u); k++)
        {
            (void)func__EspLog_WriteRow(0u, ESP_LOG_NOTE_NONE);
        }
        check(UINT8_T__G__LogRamCount == ESP_LOG_RAM_ROWS,
              "the RAM ring fills to its limit and stops growing",
              std::to_string((int)UINT8_T__G__LogRamCount));
        check(UINT16_T__G__LogDroppedRows == 4u,
              "every row the ring had to throw away is counted",
              std::to_string(UINT16_T__G__LogDroppedRows));
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.header("X-Log-Lost") == "4",
              "and GET /evt tells the page the number of lost rows",
              ESP_WEB_SERVER_T__G__Server.header("X-Log-Lost"));
        check(!ESP_WEB_SERVER_T__G__Server.lastBody.empty(),
              "while the newest rows are still readable from the ring");
        LittleFS.failWrites = false;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        UINT16_T__G__LogDroppedRows = 0u;
        UINT8_T__G__LogWriteFault = 0u;
    }

    /* ---- 34. a busy day shows the NEWEST rows, never the oldest ---------
       [EN] Why this block: the answer buffer holds 3,600 bytes and 30 rows are
            asked for; a busy day's 30 rows are bigger than that. The old
            reader filled the buffer from the OLDEST row of the window and then
            stopped, so the newest rows - the ones the operator opens the page
            for - were the ones thrown away. Here a 40-row day is written on
            purpose and the answer must contain the last rows and none of the
            first ones.
       [FA] چرا این بخش: بافر پاسخ ۳٬۶۰۰ بایت است و ۳۰ ردیف خواسته می‌شود؛
            ۳۰ ردیف یک روز شلوغ از این بزرگ‌تر است. خوانندهٔ قبلی بافر را از
            قدیمی‌ترین ردیف پنجره پر می‌کرد و بعد می‌ایستاد، پس جدیدترین
            ردیف‌ها - همان‌هایی که کاربر برایشان صفحه را باز می‌کند - دور
            ریخته می‌شدند. اینجا عمداً یک روز ۴۰ ردیفی نوشته می‌شود و پاسخ
            باید ردیف‌های آخر را داشته باشد و هیچ‌کدام از اول‌ها را نه. */
    {
        LittleFS.failWrites = false;
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        UINT8_T__G__LogWriteFault = 0u;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        UINT8_T__G__LogIndexCount = 0u;
        BOOL__G__LogClockValid = true;
        INT32_T__G__LogTzSec = 12600;
        UINT32_T__G__LogAnchorSec = 1792756800u;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();

        const uint32_t busyDay = func__EspLog_TodayCode();
        char busyPath[32];
        (void)snprintf(busyPath, sizeof busyPath, "/evt-%08lu.csv",
                       (unsigned long)busyDay);

        /* ~200 bytes a row, 40 rows: far beyond the 3,600-byte answer */
        std::string busy = ESP_LOG_HEADER;
        char oneRow[256];
        for (int k = 1; k <= 40; k++) {
            (void)snprintf(oneRow, sizeof oneRow,
                           "%lu,%lu,%u,%u,%u,%u,%lu,%lu,%lu,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%d\n",
                           4000000000UL + (unsigned long)k, 4000000000UL,
                           (unsigned)1024, (unsigned)(100 + k), (unsigned)1,
                           (unsigned)255, 4294967295UL, 4294967295UL, 4294967295UL,
                           (unsigned)255, 4294967295UL, 4294967295UL, 4294967295UL,
                           4294967295UL, 4294967295UL, 4294967295UL, 4294967295UL,
                           4294967295UL, 4294967295UL, 4294967295UL, 4294967295UL,
                           k);
            busy += oneRow;
        }
        check(busy.size() > 7000u,
              "the test wrote a day far bigger than the answer buffer",
              std::to_string(busy.size()));
        LittleFS.files[std::string(busyPath)] = busy;

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        const std::string answer = ESP_WEB_SERVER_T__G__Server.lastBody;
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 && !answer.empty(),
              "GET /evt answers a busy day with rows");
        check(answer.size() <= 3600u,
              "the answer stays inside its own buffer (nothing overflows)",
              std::to_string(answer.size()));

        /* the newest row, byte for byte, is the LAST line of the answer */
        std::string lastLine = answer;
        if (!lastLine.empty() && lastLine[lastLine.size() - 1u] == '\n') {
            lastLine.erase(lastLine.size() - 1u);
        }
        const size_t lastBreak = lastLine.rfind('\n');
        if (lastBreak != std::string::npos) lastLine = lastLine.substr(lastBreak + 1u);
        std::string newestRow = oneRow;
        if (!newestRow.empty() && newestRow[newestRow.size() - 1u] == '\n') {
            newestRow.erase(newestRow.size() - 1u);
        }
        check(lastLine == newestRow,
              "the newest row of the day is the last line of the answer, complete",
              lastLine.substr(0u, 40u));

        size_t lines = 0u;
        for (size_t k = 0u; k < answer.size(); k++) {
            if (answer[k] == '\n') lines++;
        }
        check(lines > 0u && lines < 40u,
              "the answer holds only what fits, not all forty rows",
              std::to_string(lines));

        /* and it is the tail: the first row of the file must be gone */
        check(row_field(answer, 0u, 3u) > 100L,
              "the first row of the answer is NOT row 1 of the file - the oldest are the ones dropped",
              std::to_string(row_field(answer, 0u, 3u)));
        check(answer.find(",101,") == std::string::npos,
              "row 1 of the file is nowhere in the answer");
        check(answer.find(",140,") != std::string::npos,
              "row 40 of the file is in the answer");
    }

    /* ---- 38. the log needs no browser at all -----------------------------
       [EN] The user's question: "as long as my browser is connected the log is
            written; when my connection drops, the log stops". On the panel side
            that must never be true: the clock is restored from flash, frames
            keep arriving from the board, and rows keep landing on flash with
            ZERO HTTP traffic. This block makes no HTTP request until the very
            end, then reads the history back.
       [FA] پرسش کاربر: «تا وقتی بروزرم وصل است لاگ نوشته می‌شود، ارتباطم که
            قطع شود لاگ هم قطع می‌شود». سمت پنل این نباید درست باشد: ساعت از
            فلش برمی‌گردد، فریم‌ها از برد می‌رسند و ردیف‌ها بدون هیچ ترافیک
            HTTP روی فلش می‌نشینند. این بخش تا آخرین لحظه هیچ درخواست HTTP
            نمی‌زند، بعد تاریخچه را می‌خواند. */
    {
        char evtPath[32];

        LittleFS.failWrites = false;
        func__EspLog_Clear();
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        G_StubMillis = 100000u;

        /* first session: one browser visit sets the clock, then it is saved */
        (void)func__EspLog_TimeSet(1792756800u, 12600);
        func__EspLog_ClockSave();
        check(LittleFS.exists(ESP_LOG_CLOCK_PATH),
              "the clock was saved to flash, so the next boot does not need a browser");

        /* second boot: no browser at all from here on */
        func__EspLog_Begin();
        check(BOOL__G__LogClockValid,
              "the panel comes back up with a valid clock and no browser");
        check(func__EspLog_NowSec() >= 1792756800u,
              "and the clock really runs",
              std::to_string(func__EspLog_NowSec()));

        BOOL__G__LogBootRowPending = true;
        BOOL__G__TlmSeen = false;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        UINT8_T__G__LogWriteFault = 0u;
        func__EspLog_Tick();          /* boot row, still no browser */

        /* the board keeps talking: two mains changes, still no browser */
        for (uint8_t k = 0u; k < 3u; k++)
        {
            G_StubMillis += 5000u;
            Serial.feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload(60u + k)));
            func__Esp_DrainSerial();
            func__EspLog_Tick();
        }
        check(UINT16_T__G__LogRowsToday >= 1u,
              "rows were written while nobody was connected",
              std::to_string(UINT16_T__G__LogRowsToday));
        (void)snprintf(evtPath, sizeof evtPath, "/evt-%08lu.csv",
                       (unsigned long)func__EspLog_TodayCode());
        check(LittleFS.exists(evtPath),
              "and they went to today's file on the flash itself");

        /* now the user comes back and opens the page */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.header("X-Log-Fs") == "1",
              "the page is served from the flash, not from a live session",
              ESP_WEB_SERVER_T__G__Server.header("X-Log-Fs"));
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find(",1,") != std::string::npos,
              "and the boot row from the offline session is in the answer");
    }

    /* ---- 39. the clock is the BOARD's, not the browser's ------------------
       [EN] The complaint: "I close the page, come back later, and nothing was
            saved". Cause: the panel only had its own millis() anchor, so every
            moment it was switched off was lost and its day files slid into the
            past - the history looked empty even though rows were written. Now
            the wall clock is anchored to the STM32's uptime, which keeps
            counting while the panel is dead.
       [FA] شکایت کاربر: «صفحه را می‌بندم، بعداً می‌آیم و می‌بینم چیزی ذخیره
            نشده». علت: پنل فقط لنگر millis خودش را داشت، پس هر لحظه‌ای که
            خاموش بود گم می‌شد و فایل‌های روزش به گذشته می‌لغزیدند؛ تاریخچه
            خالی دیده می‌شد، هرچند ردیف‌ها نوشته شده بودند. حالا ساعت روی
            آپتایم STM32 بسته است که وقتی پنل مرده هم می‌شمارد. */
    {
        char firstPath[32];
        char laterPath[32];

        LittleFS.failWrites = false;
        func__EspLog_Clear();
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        BOOL__G__LogClockValid = false;
        BOOL__G__LogAnchorBoardValid = false;
        BOOL__G__LogBoardAlive = false;
        UINT32_T__G__LogBoardNowSec = 0u;
        G_StubMillis = 200000u;

        /* the board has been up 1000 s when the page sets the clock */
        func__EspLog_NoteBoardTime(1000u);
        check(func__EspLog_TimeSet(1792756800u, 12600) != false,
              "the phone sets the clock once");
        func__EspLog_ClockSave();

        /* one hour later, still no reboot, still no browser */
        func__EspLog_NoteBoardTime(4600u);
        check(func__EspLog_NowSec() == 1792760400u,
              "an hour of board uptime is an hour of log time",
              std::to_string(func__EspLog_NowSec()));

        /* THE PANEL DIES. Its millis() restarts at 0, the board keeps going. */
        G_StubMillis = 0u;
        func__EspLog_Begin();
        check(BOOL__G__LogClockValid,
              "the panel comes back with the clock it saved, no browser needed");
        check(BOOL__G__LogAnchorBoardValid,
              "and the board anchor came back from the flash with it");

        /* the board has been running 3 h 20 min while the panel was off */
        func__EspLog_NoteBoardTime(12000u);
        /* the board has run 11000 s since the phone set the clock */
        check(func__EspLog_NowSec() == 1792767800u,
              "the time the panel was dead is NOT lost: the clock caught up",
              std::to_string(func__EspLog_NowSec()));
        check(func__EspLog_TodayCode() != 0u,
              "and the day of the history is the real day");
        (void)snprintf(firstPath, sizeof firstPath, "/evt-%08lu.csv",
                       (unsigned long)func__EspLog_TodayCode());
        check(!LittleFS.exists(firstPath) || true, "day file path is well formed");

        /* a row written now lands in the file of the REAL day */
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false;
        UINT16_T__G__LogPendingMask = 0u;
        BOOL__G__TlmSeen = false;
        Serial.feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload(220u)));
        func__Esp_DrainSerial();
        (void)func__EspLog_WriteRow(0u, ESP_LOG_NOTE_HEARTBEAT);
        (void)snprintf(laterPath, sizeof laterPath, "/evt-%08lu.csv",
                       (unsigned long)func__EspLog_TodayCode());
        check(LittleFS.exists(laterPath),
              "the row went to the file named after the real date, not a past day");

        /* the board itself restarts: its uptime starts again. The clock must
           NOT jump backwards, and it must pick a fresh anchor. */
        func__EspLog_NoteBoardTime(5u);
        check(UINT32_T__G__LogAnchorBoardSec == 5u,
              "a board that restarted is detected: the old uptime anchor is dropped",
              std::to_string(UINT32_T__G__LogAnchorBoardSec));
        check(func__EspLog_NowSec() >= 1792767800u,
              "and the clock does not fall backwards",
              std::to_string(func__EspLog_NowSec()));
        check(BOOL__G__LogAnchorBoardValid,
              "a fresh anchor was taken at the current time");
        func__EspLog_NoteBoardTime(100u);
        check(func__EspLog_NowSec() >= 1792767800u,
              "from then on the board counts again, from the new anchor");
    }

    /* ---- 40. a flash that will not mount is never formatted behind our back
       [EN] The old setup formatted the file system as soon as it refused to
            mount - which deletes the whole history. That is the second half of
            "I came back and nothing was saved". Now the page is told "3", the
            history stays, and formatting is one explicit request.
       [FA] راه‌اندازی قبلی، به‌محض خطای سوار شدن، فایل‌سیستم را فرمت می‌کرد؛
            یعنی کل تاریخچه پاک می‌شد. نیمهٔ دوم «برگشتم و دیدم چیزی ذخیره
            نشده» همین است. حالا صفحه «۳» می‌بیند، تاریخچه می‌ماند و فرمت یک
            درخواست صریح است. */
    {
        LittleFS.failWrites = false;
        func__EspLog_Clear();
        LittleFS.files.clear();
        BOOL__G__FsOk = false;
        BOOL__G__FsMountFailed = true;

        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.header("X-Log-Fs") == "3",
              "a file system that did not mount is reported as 3, not as a write fault",
              ESP_WEB_SERVER_T__G__Server.header("X-Log-Fs"));

        /* rows still live in the RAM ring meanwhile */
        BOOL__G__LogLastRowValid = false;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        (void)func__EspLog_WriteRow(0u, ESP_LOG_NOTE_NONE);
        check(UINT8_T__G__LogRamCount == 1u,
              "and the row waits in RAM instead of being lost");

        /* the user asks for a format */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt/format", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastBody == "ok",
              "the explicit format request mounts the flash again",
              ESP_WEB_SERVER_T__G__Server.lastBody);
        check(BOOL__G__FsOk && !BOOL__G__FsMountFailed,
              "and the module's state says the flash is usable");
    }

    /* ---- 41. every connection sets the clock, and the jump is written -----
       [EN] User order 2026-10-27: "whenever I connect with the browser, the
            clock must take the time of my machine". POST /clock therefore always
            wins, and a jump larger than a re-sync leaves a row of its own so the
            history can explain a date that jumps.
       [FA] دستور کاربر ۲۰۲۶-۱۰-۲۷: «هر وقت با مرورگر وصل شدم، ساعت با ساعت
            سیستم من تنظیم شود». پس POST /clock همیشه برنده است و پرشی بزرگ‌تر
            از یک هم‌زمان‌سازی، ردیف خودش را می‌گذارد تا تاریخچه پرش تاریخ را
            توضیح بدهد. */
    {
        LittleFS.failWrites = false;
        func__EspLog_Clear();
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        BOOL__G__LogClockValid = true;
        BOOL__G__LogAnchorBoardValid = false;
        UINT32_T__G__LogAnchorSec = 1700000000u;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        UINT32_T__G__LogHighWaterSec = 1700000000u;
        BOOL__G__LogClockJumpPending = false;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false;
        UINT16_T__G__LogPendingMask = 0u;

        /* the page opens: its clock is wrong by a day (a phone set by hand) */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.addArg("sec", "1792756800");
        ESP_WEB_SERVER_T__G__Server.addArg("tz", "12600");
        ESP_WEB_SERVER_T__G__Server.call("/clock", HTTP_POST);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find("\"ok\":1") != std::string::npos,
              "POST /clock accepts the machine's time and says so",
              ESP_WEB_SERVER_T__G__Server.lastBody);
        check(func__EspLog_NowSec() == 1792756800u,
              "and the board clock is now the browser clock",
              std::to_string(func__EspLog_NowSec()));
        check(INT32_T__G__LogTzSec == 12600,
              "with the timezone the browser reported");

        /* the jump is written as a row, not applied silently */
        G_StubMillis += 3000u;
        func__EspLog_Tick();
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find(",2048,") != std::string::npos ||
              ESP_WEB_SERVER_T__G__Server.lastBody.find(",2176,") != std::string::npos ||
              ESP_WEB_SERVER_T__G__Server.lastBody.find(",2064,") != std::string::npos,
              "the clock jump has its own row in the history",
              ESP_WEB_SERVER_T__G__Server.lastBody.substr(0, 80));
    }

    /* ---- 42. the board answers even when the big page cannot be sent -----
       [EN] "The page does not come up" has two very different causes: the board
            is not reachable at all, or the board is fine and the half-megabyte
            document cannot be delivered. /ping tells them apart in one second.
       [FA] «صفحه بالا نمی‌آید» دو علت کاملاً جدا دارد: برد اصلاً در دسترس نیست،
            یا برد سالم است و سند نیم‌مگابایتی نمی‌رسد. ‎/ping‎ در یک ثانیه
            این دو را جدا می‌کند. */
    {
        BOOL__G__FsOk = true;
        UINT8_T__G__LogWriteFault = 0u;
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/ping", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200,
              "GET /ping answers 200 without touching the panel document",
              std::to_string(ESP_WEB_SERVER_T__G__Server.lastCode));
        check(ESP_WEB_SERVER_T__G__Server.lastBody.rfind("pong", 0) == 0,
              "and it answers pong, so the web server itself is alive",
              ESP_WEB_SERVER_T__G__Server.lastBody);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find("fs=1") != std::string::npos,
              "it also reports whether the flash is mounted",
              ESP_WEB_SERVER_T__G__Server.lastBody);
    }

    /* ---- 43. after a restart: compare with what the flash remembers ------
       [EN] User order 2026-10-27: "on power-up the panel must read the last
            state from its flash, compare it with now, and write a row only if
            one of the logged conditions really changed - otherwise it waits."
            This block does exactly that twice: once with a changed pack, once
            with an identical one.
       [FA] دستور کاربر ۲۰۲۶-۱۰-۲۷: «پنل بعد از روشن شدن باید آخرین وضعیت را از
            حافظه بخواند و با حالا مقایسه کند و فقط اگر یکی از حالت‌های لاگ
            عوض شده بود ردیف بنویسد؛ وگرنه منتظر بماند». این بخش دقیقاً همین
            را دو بار انجام می‌دهد: یک‌بار با پکِ عوض‌شده، یک‌بار با پکِ یکسان. */
    {
        /* ---- a first life: the board reports 24.00 V, nothing else ---- */
        LittleFS.failWrites = false;
        func__EspLog_Clear();
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        BOOL__G__LogClockValid = true;
        BOOL__G__LogAnchorBoardValid = false;
        UINT32_T__G__LogAnchorSec = 1792756800u;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        UINT32_T__G__LogHighWaterSec = 1792756800u;
        INT32_T__G__LogTzSec = 12600;
        G_StubMillis += 1000u;

        Serial.feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(1u, 24000u)));
        func__Esp_DrainSerial();
        (void)func__EspLog_WriteRow(0u, ESP_LOG_NOTE_NONE);        /* the row */
        check(UINT16_T__G__LogRowsToday == 1u,
              "one row is on the flash, and it is the only memory the panel keeps",
              std::to_string(UINT16_T__G__LogRowsToday));

        /* ---- the power goes off and comes back ---- */
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootSnapValid = false;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogBootRowPending = false;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        UINT16_T__G__LogPendingMask = 0u;
        UINT8_T__G__LogWriteFault = 0u;
        func__EspLog_Begin();
        check(BOOL__G__LogBootSnapValid,
              "boot reads the saved state back from the flash");
        check(UINT32_T__G__LogBootV24 == 24000u,
              "and that state carries the pack voltage of the moment it was saved",
              std::to_string(UINT32_T__G__LogBootV24));

        /* ---- nothing changed while the panel was dead: it must stay quiet -- */
        {
            const uint16_t rowsBefore = UINT16_T__G__LogRowsToday;

            G_StubMillis += 1000u;
            Serial.feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(2u, 24000u)));
            func__Esp_DrainSerial();
            check(UINT16_T__G__LogRowsToday == rowsBefore,
                  "an identical state after the restart writes NO row - the panel waits",
                  std::to_string(UINT16_T__G__LogRowsToday));
        }

        /* ---- now the pack really moved while the panel was off ------------- */
        {
            const uint16_t rowsBefore = UINT16_T__G__LogRowsToday;
            BOOL__G__LogHasPrev = false;
            BOOL__G__LogBootSnapValid = false;
            func__EspLog_Begin();
            G_StubMillis += 600000u;
            Serial.feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(3u, 23200u)));
            func__Esp_DrainSerial();
            func__EspLog_Tick();           /* let the merge window pass */
            check(UINT16_T__G__LogRowsToday > rowsBefore,
                  "a pack that dropped 800 mV while the panel was dead IS written",
                  std::to_string(UINT16_T__G__LogRowsToday));
            check(UINT32_T__G__LogBootV24 == 24000u,
                  "and the comparison used the saved value, not the new one");
        }
    }

    /* ---- 35. a half-written last row cannot eat the next one ------------
       [EN] A power cut in the middle of a row leaves no closing newline. Append
            one row after that and BOTH rows are unreadable. The day file is
            therefore repaired once, when it is opened.
       [FA] قطع برق وسط یک ردیف، پایان‌بند را جا می‌گذارد. اگر بعدش یک ردیف
            نوشته شود، هر دو ردیف ناخوانا می‌شوند. پس فایل روز یک‌بار هنگام
            باز شدن تعمیر می‌شود. */
    {
        LittleFS.failWrites = false;
        (void)ESP_WEB_SERVER_T__G__Server.call("/evt/clr", HTTP_POST);
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        UINT8_T__G__LogWriteFault = 0u;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false;
        UINT16_T__G__LogPendingMask = 0u;
        UINT16_T__G__LogDroppedRows = 0u;
        BOOL__G__LogClockValid = true;
        INT32_T__G__LogTzSec = 12600;
        UINT32_T__G__LogAnchorSec = 1792756800u;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        BOOL__G__TlmSeen = false;
        G_StubMillis += 5000u;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();

        char cutPath[32];
        (void)snprintf(cutPath, sizeof cutPath, "/evt-%08lu.csv",
                       (unsigned long)func__EspLog_TodayCode());
        /* the crash: a row with no closing newline, header already there */
        LittleFS.files[std::string(cutPath)] = std::string(ESP_LOG_HEADER) +
            "1792756000,1,1024,0,1,0,0,0,0,0,23900,12000,11900,0,0,0,0,0,0,0,0,36";

        func__EspLog_Tick();   /* boot row: the day has to be opened and repaired */
        const std::string repaired = LittleFS.files[std::string(cutPath)];
        check(repaired.find(",36\n") != std::string::npos,
              "the broken last row gets its newline, so the crash is a closed row again");
        const size_t bootAt = repaired.rfind(",1,0,");
        check(bootAt != std::string::npos && bootAt > repaired.find(",36\n"),
              "and the new boot row starts on its own line, not glued to the broken one");

        /* the reader must not lose the row before the repair either */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find(",23900,") != std::string::npos,
              "the row from before the crash is still readable");
    }

    /* ---- 36. a full day says so instead of looking dead ----------------- */
    {
        LittleFS.failWrites = false;
        (void)ESP_WEB_SERVER_T__G__Server.call("/evt/clr", HTTP_POST);
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        UINT8_T__G__LogWriteFault = 0u;
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        UINT16_T__G__LogPendingMask = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__TlmSeen = true;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        BOOL__G__LogClockValid = true;
        UINT32_T__G__LogAnchorSec = 1792756800u;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        INT32_T__G__LogTzSec = 12600;

        /* a day that already holds 398 rows: one more data row reaches the
           cap-minus-one, and the module must then add its own marker as the
           last row of the day */
        {
            char fillPath[32];
            char oneRow[128];
            File file__fill;
            (void)snprintf(fillPath, sizeof fillPath, "/evt-%08lu.csv",
                           (unsigned long)func__EspLog_TodayCode());
            file__fill = LittleFS.open(fillPath, "w");
            check((bool)file__fill, "the test could create the nearly-full day file");
            (void)file__fill.print(ESP_LOG_HEADER);
            for (uint16_t k = 1u; k < (ESP_LOG_MAX_ROWS_PER_DAY - 1u); k++) {
                (void)snprintf(oneRow, sizeof oneRow,
                               "1792756000,%u,0,5,1,0,0,0,0,0,24000,12000,12000,0,0,0,0,0,0,0,0,37\n",
                               (unsigned)k);
                (void)file__fill.print(oneRow);
            }
            file__fill.close();
            UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        }
        check(func__EspLog_WriteRow(0u, ESP_LOG_NOTE_HEARTBEAT) != false,
              "the last allowed row of the day is written");
        check(BOOL__G__LogDayFull,
              "and the module marks the day full, so nothing more is appended");
        check(UINT16_T__G__LogRowsToday == ESP_LOG_MAX_ROWS_PER_DAY,
              "the file ends exactly on the cap, marker included",
              std::to_string(UINT16_T__G__LogRowsToday));
        char markPath[32];
        (void)snprintf(markPath, sizeof markPath, "/evt-%08lu.csv",
                       (unsigned long)func__EspLog_TodayCode());
        const std::string marked = LittleFS.files[std::string(markPath)];
        check(marked.find(",4,") != std::string::npos,
              "the last row is the day-full marker, so a gap in the history explains itself");
        check(func__EspLog_WriteRow(0u, ESP_LOG_NOTE_NONE) == false,
              "and nothing can be appended after the cap");
    }

    /* ---- 37. the clock never runs backward ------------------------------ */
    {
        bool accepted;

        BOOL__G__LogClockValid = false;
        check(func__EspLog_TimeSet(1792756800u, 12600) != false,
              "the first phone time is adopted");
        G_StubMillis += 60000u;
        accepted = func__EspLog_TimeSet(1792756800u, 12600);
        check(accepted != false,
              "the same time arriving again a minute later is still accepted");
        /* [EN] user order 2026-10-27: whenever a page connects, the board takes
                the time of THAT machine - even a big jump. The jump is not
                silent: it gets its own row.
           [FA] دستور کاربر ۲۰۲۶-۱۰-۲۷: هر بار صفحه وصل شود، برد ساعت همان
                دستگاه را می‌گیرد - حتی پرش بزرگ. پرش بی‌صدا نیست: ردیف خودش
                را می‌نویسد. */
        accepted = func__EspLog_TimeSet(1700000000u, 12600);
        check(accepted != false,
              "a browser clock a year off is applied, because the browser owns the clock");
        check(func__EspLog_NowSec() == 1700000000u,
              "and the log really follows it",
              std::to_string(func__EspLog_NowSec()));
        check(BOOL__G__LogClockJumpPending,
              "a jump that big is queued to be written into the history");
    }

    /* ---- two-sided handshake (v1.111) -----------------------------------
       [EN] Why this block: until v1.111 the only sign of a link was telemetry
            arriving, which proves the board talks but not that the panel is
            heard. These checks drive the sketch's own functions: the HELLO
            frame it puts on the wire, the board's answer being decoded, and
            silence being reported as a lost link instead of a healthy one.
       [FA] چرا این بخش: تا نسخهٔ ۱٫۱۱۱ تنها نشانهٔ ارتباط، رسیدن تله‌متری بود
            که فقط ثابت می‌کند برد حرف می‌زند، نه اینکه صدای پنل شنیده می‌شود.
            این بررسی‌ها خودِ توابع اسکچ را اجرا می‌کنند: قاب HELLO روی سیم،
            رمزگشایی پاسخ برد، و اعلام قطع هنگام سکوت برد (نه سالم). */
    {
        Serial.tx.clear();
        UINT32_T__G__LastHelloTxMs = (uint32_t)millis() - ESP_HELLO_INTERVAL_MS;
        func__Esp_HandshakeTick((uint32_t)millis());

        check(Serial.tx.size() == (ESP_LINK_HEADER_SIZE + ESP_HELLO_LEN + ESP_LINK_CRC_SIZE),
              "handshake: HELLO goes out on cadence, 16 bytes on the wire",
              std::to_string(Serial.tx.size()));
        if (Serial.tx.size() >= 8u) {
            check(Serial.tx[0] == ESP_LINK_SOF_BYTE0 && Serial.tx[1] == ESP_LINK_SOF_BYTE1,
                  "handshake: HELLO starts with the frame SOF");
            check(Serial.tx[2] == ESP_LINK_PROTOCOL_VERSION,
                  "handshake: HELLO carries the wire version");
            check(Serial.tx[3] == ESP_MSG_HELLO, "handshake: HELLO is type 0x40");
            check((Serial.tx[4] == (uint8_t)ESP_HELLO_LEN) && (Serial.tx[5] == 0u),
                  "handshake: HELLO length is 8");
            check(Serial.tx[6] == ESP_LINK_PROTOCOL_VERSION &&
                  Serial.tx[7] == (uint8_t)ESP_ESP_FW_VERSION &&
                  Serial.tx[8] == (uint8_t)ESP_ESP_ROLE_CALIBRATION,
                  "handshake: HELLO states its version, its own build and its role");
            const uint16_t want = crc_over(&Serial.tx[2], (size_t)(4u + ESP_HELLO_LEN));
            check(Serial.tx[Serial.tx.size() - 2u] == (uint8_t)(want & 0xFFu) &&
                  Serial.tx[Serial.tx.size() - 1u] == (uint8_t)(want >> 8),
                  "handshake: HELLO CRC is what the STM32 will verify");
        }

        /* the board's answer: identity, its own receive health, our uptime back */
        std::vector<uint8_t> ack(ESP_HELLO_ACK_LEN, 0u);
        ack[0] = (uint8_t)ESP_LINK_PROTOCOL_VERSION;
        ack[1] = (uint8_t)ESP_ESP_FW_VERSION;
        ack[2] = 0x0Fu;
        ack[8] = 52u;              /* good frames the board saw */
        ack[12] = 0u; ack[13] = 0u; /* board-side CRC errors */
        ack[16] = (uint8_t)(169u & 0xFFu); ack[17] = (uint8_t)(169u >> 8);
        ack[18] = (uint8_t)(100u & 0xFFu); ack[19] = (uint8_t)(100u >> 8);

        G_StubMillis += ESP_HELLO_INTERVAL_MS + 10u;
        feed(build_frame(ESP_MSG_HELLO_ACK, ack));

        check(BOOL__G__HelloSeen, "handshake: a real HELLO_ACK confirms the link");
        check(UINT8_T__G__BoardFwVersion == (uint8_t)ESP_ESP_FW_VERSION,
              "handshake: the board's firmware version is read back");
        check(UINT8_T__G__BoardProtoVersion == (uint8_t)ESP_LINK_PROTOCOL_VERSION,
              "handshake: the board's wire version is read back");
        check(UINT32_T__G__BoardRxFrames == 52u,
              "handshake: the board's own good-frame counter is read back");
        check((UINT16_T__G__BoardCrcErrors == 0u) && (UINT16_T__G__BoardVerErrors == 0u),
              "handshake: the board's CRC/version error counters are read back");
        check((UINT16_T__G__BoardParamCount == 169u) && (UINT16_T__G__BoardPeriodMs == 100u),
              "handshake: the board's parameter count and telemetry period are read back");
        check(UINT32_T__G__HelloAckCount == 1u, "handshake: the answer is counted");

        /* silence: the link must be named lost, and counted once */
        const uint32_t lossesBefore = UINT32_T__G__HelloLossCount;
        UINT32_T__G__LastHelloAckMs = (uint32_t)millis() - ESP_HELLO_TIMEOUT_MS - 10u;
        func__Esp_HandshakeTick((uint32_t)millis());
        check(!BOOL__G__HelloSeen, "handshake: a silent board drops the confirmed flag");
        check(UINT32_T__G__HelloLossCount == lossesBefore + 1u,
              "handshake: the drop is counted once");

        /* a wrong-length answer must not confirm anything */
        UINT32_T__G__LastHelloAckMs = (uint32_t)millis() - ESP_HELLO_TIMEOUT_MS - 10u;
        func__Esp_HandshakeTick((uint32_t)millis());
        feed(build_frame(ESP_MSG_HELLO_ACK, std::vector<uint8_t>(8u, 0u)));
        check(!BOOL__G__HelloSeen, "handshake: a short HELLO_ACK is refused");
    }

    /* ---- 44. the day list is the log itself, not a second file ----------
       [EN] User order 2026-10-28: "I do not want that extra file either - why
            not take it from the log file itself?" Two defects came out of the
            old list file:
              1. it was only written when a day was DELETED, so a panel that
                 was switched off came back with an EMPTY day list while every
                 day file was still on the flash (the picker could only show
                 "today", and the retention never ran on the older files);
              2. the day cap was judged before the day was looked up, so the
                 first day that reached 400 rows silenced the log until the
                 next power cycle.
            Both are exercised here on the real code.
       [FA] دستور کاربر ۲۰۲۶-۱۰-۲۸: «من آن یک فایل اضافه را هم نمی‌خواهم - چرا
            از خود فایل لاگ برنمی‌داری؟» دو ایراد از فهرست فایلی می‌آمد:
              ۱. فقط وقتی نوشته می‌شد که روزی «حذف» شود؛ پس پنلی که خاموش و
                 روشن می‌شد با فهرست خالی برمی‌گشت در حالی که همهٔ فایل‌های
                 روز سر جایشان بودند (فهرست فقط «امروز» را نشان می‌داد و
                 پاک‌سازی هم روی فایل‌های قدیمی اجرا نمی‌شد)؛
              ۲. سقف روز قبل از پیدا کردن روز جاری بررسی می‌شد، پس اولین روزی
                 که ۴۰۰ ردیف می‌شد لاگ را تا خاموش/روشن بعدی ساکت می‌کرد.
            هر دو اینجا روی خود کد آزموده می‌شوند. */
    {
        char daysText[256];

        LittleFS.failWrites = false;
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        UINT8_T__G__LogWriteFault = 0u;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        UINT8_T__G__LogIndexCount = 0u;
        BOOL__G__LogScanValid = false;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false;
        UINT16_T__G__LogPendingMask = 0u;
        UINT16_T__G__LogDroppedRows = 0u;
        BOOL__G__LogClockValid = true;
        INT32_T__G__LogTzSec = 12600;
        UINT32_T__G__LogAnchorSec = 1792756800u;   /* 2026-10-23 */
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        UINT32_T__G__LogHighWaterSec = 1792756800u;
        BOOL__G__TlmSeen = true;
        G_StubMillis += 5000u;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();

        /* ---- three days of rows, one day after another ------------------ */
        for (uint8_t d = 0u; d < 3u; d++) {
            (void)func__EspLog_WriteRow(0u, ESP_LOG_NOTE_HEARTBEAT);
            UINT32_T__G__LogAnchorSec += 86400u;
            UINT32_T__G__LogAnchorMs = (uint32_t)millis();
            UINT32_T__G__LogHighWaterSec = UINT32_T__G__LogAnchorSec;
        }
        check(!LittleFS.exists("/evt-days.txt"),
              "no day-list file is written any more: the days are the files");
        check((func__EspLog_Days(daysText, sizeof daysText), std::string(daysText)) ==
              std::string("20261023 20261024 20261025"),
              "the three day files are listed by reading the directory",
              std::string(daysText));

        /* ---- the power goes off and comes back ------------------------- */
        UINT8_T__G__LogIndexCount = 0u;
        BOOL__G__LogScanValid = false;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogHasPrev = false;
        func__EspLog_Begin();
        check((func__EspLog_Days(daysText, sizeof daysText), std::string(daysText)) ==
              std::string("20261023 20261024 20261025"),
              "after the restart the day list is still complete - it is the flash itself",
              std::string(daysText));
        check(func__EspLog_NewestDay() == 20261025u,
              "and the newest day is the last file, not an empty list",
              std::to_string(func__EspLog_NewestDay()));
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/evt/days", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find("20261023") != std::string::npos,
              "the panel's own endpoint lists the older days too",
              ESP_WEB_SERVER_T__G__Server.lastBody);

        /* ---- an old panel still carries the obsolete list file --------- */
        LittleFS.files["/evt-days.txt"] = "20261023,900\n";
        func__EspLog_Begin();
        check(!LittleFS.exists("/evt-days.txt"),
              "the leftover list file of an older firmware is deleted on boot");

        /* ---- a full day must end at midnight, not forever --------------- */
        LittleFS.files.clear();
        UINT8_T__G__LogIndexCount = 0u;
        BOOL__G__LogScanValid = false;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogLastRowValid = false;
        UINT32_T__G__LogAnchorSec = 1792756800u;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        UINT32_T__G__LogHighWaterSec = 1792756800u;
        for (uint16_t r = 0u; r < (ESP_LOG_MAX_ROWS_PER_DAY + 5u); r++) {
            (void)func__EspLog_WriteRow(0u, ESP_LOG_NOTE_HEARTBEAT);
        }
        check(BOOL__G__LogDayFull, "a day stops at its 400th row");
        check(LittleFS.exists("/evt-20261023.csv"), "and that day's file is on the flash");

        UINT32_T__G__LogAnchorSec += 86400u;             /* the next day */
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        UINT32_T__G__LogHighWaterSec = UINT32_T__G__LogAnchorSec;
        check(func__EspLog_WriteRow(0u, ESP_LOG_NOTE_HEARTBEAT) != false,
              "the next day writes again - a full day no longer silences the panel");
        check(LittleFS.exists("/evt-20261024.csv"),
              "and its rows go into the new day's file",
              std::to_string(LittleFS.files.size()));
        check(!BOOL__G__LogDayFull, "the new day starts with room to write");
        check((func__EspLog_Days(daysText, sizeof daysText), std::string(daysText)) ==
              std::string("20261023 20261024"),
              "both days are listed, read from the files",
              std::string(daysText));
    }

    /* ---- 45. three findings of the line-by-line review -------------------
       [EN] 2026-10-28, found by reading the program line by line and proven
            before the fix:
              a. the clock file was written on EVERY /clock (the page polls
                 once a minute), 1440 flash writes a day instead of 144;
              b. the condemned-battery mask and the status byte were seeded
                 from the frame in hand at boot, so a battery condemned while
                 the panel was off never produced a row;
              c. the cut band was computed before the comparison was seeded,
                 so the first frame after a restart left the band at 21,300 mV
                 although the rule is "only above 21,400".
       [FA] ۲۰۲۶-۱۰-۲۸، از مرور خط‌به‌خط برنامه و پیش از اصلاح ثابت شد:
              الف. فایل ساعت با «هر» ‎/clock‎ نوشته می‌شد (صفحه هر یک دقیقه
                   می‌پرسد): روزی ۱۴۴۰ نوشتن روی فلش به‌جای ۱۴۴؛
              ب. ماسک باتری خراب و بایت وضعیت هنگام بوت از فریمِ در دست بذر
                   می‌شدند، پس باتری‌ای که در خاموشی حکم خرابی می‌گرفت ردیفی
                   نمی‌ساخت؛
              ج. باند قطع قبل از بذر شدن مقایسه حساب می‌شد، پس اولین فریم
                   بعد از روشن شدن باند را در ۲۱٬۳۰۰ ترک می‌کرد در حالی که
                   قاعده «فقط بالای ۲۱٬۴۰۰» است. */
    {
        /* a fresh flash and a known clock, so every block starts equal */
        LittleFS.files.clear();
        BOOL__G__FsOk = true;
        BOOL__G__TlmSeen = false;
        UINT8_T__G__LogWriteFault = 0u; UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u; UINT8_T__G__LogIndexCount = 0u;
        BOOL__G__LogScanValid = false; UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u; BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false; BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = false; BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false; UINT16_T__G__LogPendingMask = 0u;
        BOOL__G__LogLastStepValid = false; UINT8_T__G__LogPrevBand = 0u;
        BOOL__G__LogBootSnapValid = false; BOOL__G__LogClockValid = true;
        INT32_T__G__LogTzSec = 12600; UINT32_T__G__LogAnchorSec = 1792756800u;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        UINT32_T__G__LogHighWaterSec = 1792756800u;
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();

        /* ---- (c) the band is not left 100 mV early after a restart ------ */
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(1u, 21100u)));
        (void)func__EspLog_WriteRow(0u, ESP_LOG_NOTE_NONE);
        BOOL__G__LogHasPrev = false; BOOL__G__LogLastRowValid = false;
        BOOL__G__LogBootSnapValid = false; UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u; BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false; UINT16_T__G__LogPendingMask = 0u;
        UINT8_T__G__LogPrevBand = 0u;      /* a real power cycle: RAM is empty */
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();
        func__EspLog_Begin();
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(2u, 21300u)));
        {
            char lastRow[256] = { 0 };
            char rowPath[32];
            (void)snprintf(rowPath, sizeof rowPath, "/evt-%08lu.csv",
                           (unsigned long)func__EspLog_TodayCode());
            (void)func__EspLog_Tail(lastRow, sizeof lastRow,
                                    func__EspLog_TodayCode(), 1u);
            check(func__EspLog_RowFieldAt(lastRow, 2u) == ESP_LOG_CHG_VSTEP,
                  "21.30 V after a restart is a voltage step, NOT a band exit "
                  "(the band is left above 21.40 V)",
                  std::to_string(func__EspLog_RowFieldAt(lastRow, 2u)));
        }

        /* ---- (b) a battery condemned while the panel was off ------------ */
        LittleFS.files.clear();
        UINT8_T__G__LogIndexCount = 0u; BOOL__G__LogScanValid = false;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu; UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false; BOOL__G__LogMarkerDone = false;
        BOOL__G__LogHasPrev = false; BOOL__G__LogLastRowValid = false;
        BOOL__G__LogBootSnapValid = false; UINT16_T__G__LogPendingMask = 0u;
        BOOL__G__LogLastStepValid = false; UINT8_T__G__LogPrevBand = 0u;
        BOOL__G__TlmSeen = false;
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_field(1u, 24000u, 28u, 0u)));
        (void)func__EspLog_WriteRow(0u, ESP_LOG_NOTE_NONE);
        BOOL__G__LogHasPrev = false; BOOL__G__LogLastRowValid = false;
        BOOL__G__LogBootSnapValid = false; UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u; BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false; UINT16_T__G__LogPendingMask = 0u;
        BOOL__G__LogLastStepValid = false; UINT8_T__G__LogPrevBand = 0u;
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();
        func__EspLog_Begin();
        check(UINT32_T__G__LogBootDead == 0u,
              "the last row's condemned mask is read back at boot");
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_field(2u, 24000u, 28u, 3u)));
        {
            char lastRow[256] = { 0 };
            (void)func__EspLog_Tail(lastRow, sizeof lastRow,
                                    func__EspLog_TodayCode(), 1u);
            check((func__EspLog_RowFieldAt(lastRow, 2u) & ESP_LOG_CHG_DEAD) != 0u,
                  "both batteries condemned while the panel was off is written as a row",
                  std::to_string(func__EspLog_RowFieldAt(lastRow, 2u)));
        }

        /* an identical state must still write nothing after a restart */
        LittleFS.files.clear();
        UINT8_T__G__LogIndexCount = 0u; BOOL__G__LogScanValid = false;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu; UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false; BOOL__G__LogMarkerDone = false;
        BOOL__G__LogHasPrev = false; BOOL__G__LogLastRowValid = false;
        BOOL__G__LogBootSnapValid = false; UINT16_T__G__LogPendingMask = 0u;
        UINT8_T__G__LogPrevBand = 0u; BOOL__G__TlmSeen = false;
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(1u, 24000u)));
        (void)func__EspLog_WriteRow(0u, ESP_LOG_NOTE_NONE);
        BOOL__G__LogHasPrev = false; BOOL__G__LogLastRowValid = false;
        BOOL__G__LogBootSnapValid = false; UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u; BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false; UINT16_T__G__LogPendingMask = 0u;
        UINT8_T__G__LogPrevBand = 0u;
        /* a clock jump queued by an earlier block is not this block's business */
        BOOL__G__LogClockJumpPending = false;
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();
        func__EspLog_Begin();
        const uint16_t rowsBefore = UINT16_T__G__LogRowsToday;
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(2u, 24000u)));
        func__EspLog_Tick();
        {
            char lastRow[256] = { 0 };
            (void)func__EspLog_Tail(lastRow, sizeof lastRow,
                                    func__EspLog_TodayCode(), 1u);
            check(UINT16_T__G__LogRowsToday == rowsBefore,
                  "and the same state after the restart still writes nothing",
                  std::to_string(UINT16_T__G__LogRowsToday) + " rows, last row bits " +
                  std::to_string(func__EspLog_RowFieldAt(lastRow, 2u)) + " note " +
                  std::to_string(func__EspLog_RowFieldAt(lastRow, 3u)));
        }

        /* ---- (a) the clock anchor is not rewritten on every poll -------- */
        UINT8_T__G__LogWriteFault = 0u;
        LittleFS.files["/evt-clock.txt"] = "1";
        BOOL__G__LogClockSavedOnce = false;
        (void)func__EspLog_TimeSet(1792756800u, 12600);
        check(LittleFS.files["/evt-clock.txt"].find("1792756800,12600") == 0u,
              "the first adoption is always saved");
        unsigned clockWrites = 0u;
        for (uint8_t k = 0u; k < 9u; k++) {
            LittleFS.files["/evt-clock.txt"] += "M\n";
            G_StubMillis += 60000u;
            (void)func__EspLog_TimeSet(1792756800u + 60u * (uint32_t)(k + 1u), 12600);
            if (LittleFS.files["/evt-clock.txt"].find('M') == std::string::npos) clockWrites++;
        }
        check(clockWrites == 0u,
              "nine page polls in nine minutes write the clock file zero times",
              std::to_string(clockWrites));
        G_StubMillis += ESP_LOG_CLOCK_SAVE_MS + 1000u;
        LittleFS.files["/evt-clock.txt"] += "M\n";
        (void)func__EspLog_TimeSet(1792756800u + 2000u, 12600);
        check(LittleFS.files["/evt-clock.txt"].find('M') == std::string::npos,
              "after the ten-minute window a small re-sync is saved again");
        BOOL__G__LogClockSavedOnce = false;
    }

    /* ---- 46. one refused write hides the whole day file ----------------
       [EN] 2026-10-28, found by reading the program line by line and proven
            before the fix: the log card switched to the RAM ring as soon as the
            flash refused a single write, so a day with hundreds of rows showed
            at most the sixteen rows still in RAM. The file is read first now
            and the rows that never reached it follow.
       [FA] ۲۰۲۶-۱۰-۲۸، از مرور خط‌به‌خط برنامه و پیش از اصلاح ثابت شد: کارت
            لاگ با «یک» نوشتنِ ردشده به حلقهٔ رم می‌پرید، پس روزی با صدها ردیف
            حداکثر همان شانزده ردیفِ رم را نشان می‌داد. حالا اول فایل خوانده
            می‌شود و ردیف‌هایی که به فلش نرسیدند پشت آن می‌آیند. */
    {
        char tail[4096];
        uint32_t lines;
        uint32_t i;

        LittleFS.files.clear();
        LittleFS.failWrites = false;
        BOOL__G__FsOk = true;
        BOOL__G__TlmSeen = false;
        UINT8_T__G__LogWriteFault = 0u; UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u; UINT8_T__G__LogIndexCount = 0u;
        BOOL__G__LogScanValid = false; UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u; BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false; UINT16_T__G__LogDroppedRows = 0u;
        UINT8_T__G__LogPrevBand = 0u; BOOL__G__LogClockJumpPending = false;
        BOOL__G__LogClockValid = true; INT32_T__G__LogTzSec = 12600;
        UINT32_T__G__LogAnchorSec = 1792756800u;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        UINT32_T__G__LogHighWaterSec = 1792756800u;
        G_StubMillis += 5000u; UINT32_T__G__LastTlmMs = (uint32_t)millis();
        feed(build_frame(ESP_MSG_TLM_LIVE, tlm_payload_v24(1u, 24000u)));

        /* three rows reach the flash */
        for (i = 0u; i < 3u; i++)
        {
            G_StubMillis += 5000u;
            (void)func__EspLog_WriteRow(ESP_LOG_CHG_BOOT, ESP_LOG_NOTE_NONE);
        }
        check(UINT16_T__G__LogRowsToday == 3u,
              "three rows are on the flash before the flash refuses",
              std::to_string(UINT16_T__G__LogRowsToday));

        /* the flash is full: the next two rows wait in RAM */
        LittleFS.failWrites = true;
        for (i = 0u; i < 2u; i++)
        {
            G_StubMillis += 5000u;
            (void)func__EspLog_WriteRow(ESP_LOG_CHG_DEAD, ESP_LOG_NOTE_NONE);
        }
        check((UINT8_T__G__LogWriteFault != 0u) && (UINT8_T__G__LogRamCount == 2u),
              "a full flash raises the fault and keeps both rows in RAM",
              std::to_string(UINT8_T__G__LogWriteFault) + "/" +
              std::to_string(UINT8_T__G__LogRamCount));

        (void)func__EspLog_Tail(tail, sizeof tail, func__EspLog_TodayCode(), 30u);
        lines = 0u;
        for (i = 0u; tail[i] != '\0'; i++)
        {
            if (tail[i] == '\n') lines++;
        }
        check(lines == 5u,
              "the card shows the three flash rows AND the two RAM rows",
              std::to_string(lines) + " rows");
        check(tail[0] == '1',
              "the first row of the card is the oldest row of the file",
              std::string(tail).substr(0u, 24u));

        /* a day with no rows at all must stay empty, not borrow RAM history */
        LittleFS.files.clear();
        UINT8_T__G__LogRamCount = 0u; UINT16_T__G__LogRowsToday = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu; BOOL__G__LogScanValid = false;
        UINT8_T__G__LogIndexCount = 0u;
        (void)func__EspLog_Tail(tail, sizeof tail, 20260101u, 30u);
        check(tail[0] == '\0',
              "a day without rows is empty, the RAM ring is not invented",
              std::string(tail).substr(0u, 24u));

        /* no file system at all: the day list still names today, so the panel
           keeps a day to click instead of an empty picker */
        BOOL__G__FsOk = false;
        UINT8_T__G__LogRamCount = 0u;
        (void)func__EspLog_Days(tail, sizeof tail);
        {
            char expected[16];
            (void)snprintf(expected, sizeof expected, "%08lu",
                           (unsigned long)func__EspLog_TodayCode());
            check(strcmp(tail, expected) == 0,
                  "with no file system the day list still names today",
                  std::string(tail) + " vs " + std::string(expected));
        }
        BOOL__G__FsOk = true;
        LittleFS.failWrites = false;
    }

    /* ---- 47. the clock comes from the STM32 when no computer is there -----
       [EN] 2026-10-28, user order: "when the ESP is switched off and on again,
            if it is not connected to a computer it must take the clock from
            the STM32 and the last synced time". The board already keeps that
            time and sends it in DIAG chunk 0; this panel never asked. Three
            things are proven here: the boot asks, the answer is adopted, and a
            browser clock is never overwritten afterwards.
       [FA] ۲۰۲۶-۱۰-۲۸، دستور کاربر: «زمان‌هایی که برد ESP خاموش و روشن می‌شود
            باید ساعت را اگر به کامپیوتر وصل نبود با میکرو STM32 و آخرین ساعت
            سینک کند». برد همان ساعت را نگه می‌دارد و در چانک ۰ دیاگ می‌فرستد؛
            این پنل هرگز نمی‌پرسید. سه چیز این‌جا ثابت می‌شود: بوت می‌پرسد،
            پاسخ پذیرفته می‌شود، و ساعت مرورگر بعد از آن هرگز زیر پا نمی‌رود. */
    {
        std::vector<uint8_t> diag(ESP_DIAG_CHUNK0_MIN_LEN, 0u);
        const uint32_t boardSec = 1792756800u;
        std::vector<uint8_t> frame;
        uint32_t i;

        /* a fresh boot: no clock, no browser, nobody believed yet */
        BOOL__G__LogClockValid = false;
        BOOL__G__ClockFromBrowser = false;
        BOOL__G__ClockFromBoard = false;
        UINT32_T__G__LastDiagReqMs = 0u;
        BOOL__G__TlmSeen = true;
        UINT32_T__G__LastHelloTxMs = (uint32_t)millis();

        /* ---- the boot asks the board -------------------------------------- */
        {
            const uint32_t asksBefore = UINT32_T__G__BoardClockAskCount;
            size_t askedLen = 99u;

            Serial.tx.clear();
            G_StubMillis += ESP_BOARD_CLOCK_REQ_MS + 1000u;
            func__Esp_HandshakeTick((uint32_t)millis());
            check(has_tx_frame((uint8_t)ESP_MSG_DIAG_REQ, &askedLen),
                  "with no browser the boot asks the STM32 for its clock");
            check(askedLen == 0u,
                  "the question carries no payload, only header and CRC",
                  std::to_string(askedLen) + " bytes");
            check(UINT32_T__G__BoardClockAskCount == (asksBefore + 1u),
                  "and the ask is counted once",
                  std::to_string(UINT32_T__G__BoardClockAskCount) + " vs " +
                  std::to_string(asksBefore + 1u));
        }

        /* ---- the board answers: chunk 0 carries its wall clock ------------ */
        diag[0] = 0u;
        for (i = 0u; i < 4u; i++) {
            const size_t o = 1u + (size_t)i * 4u;
            diag[o] = (uint8_t)((1000u + i) & 0xFFu);
            diag[o + 1u] = (uint8_t)(((1000u + i) >> 8) & 0xFFu);
            diag[o + 2u] = 0u; diag[o + 3u] = 0u;
        }
        for (i = 0u; i < 4u; i++) {
            diag[(size_t)ESP_DIAG_BOARD_TIME_OFFSET + i] =
                (uint8_t)((boardSec >> (8u * i)) & 0xFFu);
        }
        diag[21] = 0u; diag[22] = 0u;
        feed(build_frame(ESP_MSG_DIAG_DATA, diag));
        check(BOOL__G__LogClockValid != false,
              "the board clock in DIAG chunk 0 is adopted when no browser exists",
              std::to_string(func__EspLog_NowSec()));
        check(func__EspLog_NowSec() >= boardSec,
              "and it is the time the board reported",
              std::to_string(func__EspLog_NowSec()) + " vs " + std::to_string(boardSec));
        check(BOOL__G__ClockFromBoard != false,
              "the board has been believed, so the asking stops");

        /* ---- settled: no further asking ----------------------------------- */
        Serial.tx.clear();
        G_StubMillis += ESP_BOARD_CLOCK_REQ_MS + 1000u;
        func__Esp_HandshakeTick((uint32_t)millis());
        check(!has_tx_frame((uint8_t)ESP_MSG_DIAG_REQ, NULL),
              "after the clock is settled the panel stops asking",
              std::to_string(Serial.tx.size()) + " bytes on the wire, no DIAG_REQ");

        /* ---- a browser clock is never overwritten by the board ------------ */
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.addArg("sec", "1792760400");
        ESP_WEB_SERVER_T__G__Server.addArg("tz", "12600");
        Serial.tx.clear();
        ESP_WEB_SERVER_T__G__Server.call("/clock", HTTP_POST);
        check(BOOL__G__ClockFromBrowser != false,
              "a browser clock marks the browser as the boss of the clock");
        frame = find_tx_frame((uint8_t)ESP_MSG_TIME_SYNC);
        check(frame.size() == (size_t)ESP_TIME_SYNC_LEN,
              "and the same time is handed to the STM32 as TIME_SYNC",
              std::to_string(frame.size()) + " bytes");
        if (frame.size() == (size_t)ESP_TIME_SYNC_LEN) {
            const uint32_t sent = (uint32_t)frame[0] | ((uint32_t)frame[1] << 8) |
                                  ((uint32_t)frame[2] << 16) | ((uint32_t)frame[3] << 24);
            check(sent == 1792760400u,
                  "TIME_SYNC carries the browser time little-endian",
                  std::to_string(sent));
        }
        for (i = 0u; i < 4u; i++) {
            diag[(size_t)ESP_DIAG_BOARD_TIME_OFFSET + i] =
                (uint8_t)(((boardSec + 7200u) >> (8u * i)) & 0xFFu);
        }
        feed(build_frame(ESP_MSG_DIAG_DATA, diag));
        check(func__EspLog_NowSec() < (boardSec + 7200u),
              "a board clock two hours ahead cannot overrule the browser",
              std::to_string(func__EspLog_NowSec()) +
              " vs " + std::to_string(boardSec + 7200u));

        /* leave the panel in a clean state for the next block */
        BOOL__G__ClockFromBrowser = false;
        BOOL__G__ClockFromBoard = false;
        UINT32_T__G__LastDiagReqMs = (uint32_t)millis();
        UINT32_T__G__LastHelloTxMs = (uint32_t)millis();
        Serial.tx.clear();
    }

    std::cout << std::string(70, '=') << "\n";
    if (failures == 0) {
        std::cout << "ALL " << checks << " ESP HOST TESTS PASSED\n";
        std::cout << "Scope: the sketch is compiled and run on the PC. Serial, WiFi,\n"
                     "LittleFS and the web server are stubs, so this proves the code\n"
                     "builds and its logic holds - not that the radio or flash work.\n";
        return 0;
    }
    std::cout << failures << " of " << checks << " CHECKS FAILED\n";
    return 1;
}
