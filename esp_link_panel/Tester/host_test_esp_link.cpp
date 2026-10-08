/**
 * @file   host_test_esp_link.cpp
 * @brief  [EN] First host tests for the ESP program.
 *
 *         The sketch itself is compiled here, unmodified: this file includes
 *         esp_link_panel.ino, so setup() and loop() and every plink_*.h the
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
 *         esp_link_panel.ino را include می‌کند، پس setup() و loop() و همهٔ
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
#include "../esp_link_panel.ino"

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
       [EN] The panel learns which parameters are still in flight from four
            32-bit masks in /t. id 96 is the first bit of the fourth word, and
            getting that boundary wrong once meant evaluating 1UL << 32:
            undefined behaviour, not an off-by-one. Both sides of all three
            boundaries are checked, by reading the JSON the browser reads.
       [FA] پنل از چهار ماسک ۳۲ بیتی در t/ می‌فهمد کدام پارامترها در راه‌اند.
            شناسهٔ ۹۶ اولین بیت کلمهٔ چهارم است و یک‌بار اشتباه در همین مرز
            یعنی 1UL << 32 - رفتار تعریف‌نشده. هر دو طرف هر سه مرز از روی
            همان JSON که مرورگر می‌خواند آزموده می‌شود. */
    {
        const uint16_t edges[] = { 0u, 31u, 32u, 63u, 64u, 95u, 96u,
                                   (uint16_t)(ESP_PARAM_COUNT - 1u) };
        for (size_t k = 0; k < sizeof edges / sizeof edges[0]; k++) {
            const uint16_t id = edges[k];
            for (uint16_t i = 0u; i < ESP_PARAM_COUNT; i++) BOOL__G__TxParamPending[i] = false;
            BOOL__G__TxParamPending[id] = true;

            ESP_WEB_SERVER_T__G__Server.call("/t", HTTP_GET);
            const std::string body = ESP_WEB_SERVER_T__G__Server.lastBody;

            /* the word this id must land in, and the bit inside it */
            /* v1.80: ids 128..131 (scenario-6 lamp/buzzer) added a fifth word */
            const int word = id / 32;                       /* 0..4 */
            const unsigned bit = (unsigned)(id % 32);
            static const char *keys[5] = { "\"q\":", "\"q2\":", "\"q3\":", "\"q4\":", "\"q5\":" };
            bool found = true;
            unsigned long seen[5] = { 0, 0, 0, 0, 0 };
            for (int w = 0; w < 5; w++) {
                const size_t at = body.find(keys[w]);
                if (at == std::string::npos) { found = false; break; }
                seen[w] = strtoul(body.c_str() + at + strlen(keys[w]), NULL, 10);
            }
            check(found, "the telemetry JSON carries all five pending masks");
            if (!found) break;
            check(seen[word] == (1UL << bit),
                  "parameter " + std::to_string(id) + " sets exactly its own bit, in word " +
                      std::to_string(word + 1),
                  "got " + std::to_string(seen[word]) + ", expected " +
                      std::to_string(1UL << bit));
            unsigned long others = 0;
            for (int w = 0; w < 5; w++) if (w != word) others |= seen[w];
            check(others == 0uL,
                  "parameter " + std::to_string(id) + " leaves the other four words clear",
                  "a spilled bit makes the panel wait forever on a parameter nobody sent");
        }
        for (uint16_t i = 0u; i < ESP_PARAM_COUNT; i++) BOOL__G__TxParamPending[i] = false;
    }

    /* ---- 9. the query-string integer parser ----------------------------- */
    {
        int32_t v = 0;
        check(func__Esp_ParseInt("650", &v) && v == 650, "a plain number parses");
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
        check(UINT32_T__G__LutReadChain2[0] == 777u &&
              UINT32_T__G__LutReadPower2[0] == 888u,
              "battery 2 chain/power values survive the wire decode");
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.call("/lut", HTTP_GET);
        check(ESP_WEB_SERVER_T__G__Server.lastBody.find("\"ready\":1") != std::string::npos &&
              ESP_WEB_SERVER_T__G__Server.lastBody.find("[777,888]") != std::string::npos,
              "GET /lut exposes the actual battery-2 pair after both frames arrive");
        feed(build_frame(ESP_MSG_LUT_DATA, lut_data(1u, {{999u, 999u}})));
        check(UINT8_T__G__LutReadCount1 == 2u && UINT32_T__G__LutReadChain1[0] == 111u,
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
