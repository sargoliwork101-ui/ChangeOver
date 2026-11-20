/* [EN] Real-sketch backup endpoint invariants and response fixtures for Chromium. */
#include "stub_arduino.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
unsigned long G_StubMillis = 1000u;
StubSerial Serial;
StubWiFi WiFi;
StubFS LittleFS;
#define ESP8266 1
#include "../ESP_Calibration.ino"
static unsigned checks = 0;
static void check(bool result, const char *message)
{
    ++checks;
    if (!result) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
    std::cout << "  ok   " << message << '\n';
}
/* [EN] JSON quoting preserves all bytes of the ASCII production responses. */
static std::string quote(const std::string &text)
{
    std::ostringstream out;
    out << '"';
    const char *hex = "0123456789abcdef";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32u) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << c;
    }
    out << '"';
    return out.str();
}
static std::string response(const char *path, int method = HTTP_GET,
                            const std::map<std::string, std::string> &args = {})
{
    auto &server = ESP_WEB_SERVER_T__G__Server;
    server.args = args;
    check(server.call(path, method), "requested route is registered by the real sketch");
    std::ostringstream out;
    out << "{\"status\":" << server.lastCode << ",\"body\":" << quote(server.lastBody) << ",\"headers\":{";
    bool first = true;
    for (const auto &header : server.headers) {
        if (!first) out << ',';
        first = false;
        out << quote(header.first) << ':' << quote(header.second);
    }
    out << "}}";
    return out.str();
}
static void seed_day(const char *path, uint32_t epoch)
{
    EspLogCodec::State state;
    std::string bytes(reinterpret_cast<const char *>(EspLogCodec::header()), EspLogCodec::HeaderSize);
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t fields[EspLogCodec::Fields] = {};
        fields[0] = epoch + i * 60u; fields[1] = i * 60u;
        fields[2] = 1024u; fields[8] = i; fields[13] = 24000u + i * 500u;
        uint8_t frame[EspLogCodec::MaxFrame];
        const size_t length = EspLogCodec::encode(state, fields, frame);
        check(length > 0, "production day codec encodes a fixture row");
        bytes.append(reinterpret_cast<const char *>(frame), length);
    }
    LittleFS.files[path] = bytes;
}
int main(int argc, char **argv)
{
    check(argc == 2, "fixture output path supplied");
    setup();
    LittleFS.files.clear();
    func__EspLog_ResetCaches();
    EspBenchCodec::reset(BENCH__G__State);
    std::ifstream source("ESP_Calibration/Tester/fixtures/bench_capture_3step_2repeat.csv");
    check(source.good(), "frozen real-panel capture fixture opens");
    std::string line, expanded = ESP_BENCHLOG_HEADER;
    while (std::getline(source, line)) {
        line += '\n'; expanded += line;
        (void)response("/benchlog/add", HTTP_POST, {{"plain", line}});
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200, "real HTTP handler accepts frozen panel capture line");
    }
    seed_day("/evt-20261101.csv", 1793491200u);
    seed_day("/evt-20261102.csv", 1793577600u);
    (void)func__EspLog_ScanDays(true);
    for (unsigned i = 0; i < ESP_PARAM_COUNT; ++i) {
        BOOL__G__ParamKnown[i] = true;
        UINT32_T__G__ParamApplied[i] = 0u;
    }
    UINT32_T__G__ParamApplied[2] = UINT32_T__G__ParamApplied[3] = 1000u;
    UINT32_T__G__ParamApplied[13] = UINT32_T__G__ParamApplied[14] = 500u;
    BOOL__G__TlmSeen = true; UINT32_T__G__LastTlmMs = G_StubMillis;
    UINT16_T__G__TlmSeq = 42u;
    const auto stored = LittleFS.files;
    Serial.tx.clear();
    std::map<std::string, std::string> fixtures;
    fixtures["/t"] = response("/t");
    fixtures["/benchlog?i=1"] = response("/benchlog", HTTP_GET, {{"i", "1"}});
    fixtures["/benchlog"] = response("/benchlog");
    check(ESP_WEB_SERVER_T__G__Server.lastBody == expanded, "real compact bench download reconstructs the complete input exactly");
    fixtures["/evt/days"] = response("/evt/days");
    check(ESP_WEB_SERVER_T__G__Server.lastBody.find("20261101") != std::string::npos &&
          ESP_WEB_SERVER_T__G__Server.lastBody.find("20261102") != std::string::npos,
          "actual history day list contains both seeded days");
    for (const char *day : {"20261101", "20261102"}) {
        fixtures[std::string("/evt/dl?day=") + day] = response("/evt/dl", HTTP_GET, {{"day", day}});
        check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
              ESP_WEB_SERVER_T__G__Server.lastBody.find("# WARNING:") == std::string::npos,
              "actual binary daily history downloads without a corruption warning");
    }
    fixtures["/evt"] = response("/evt");
    fixtures["/evt/pct"] = response("/evt/pct");
    check(Serial.tx.empty(), "backup GET endpoints emit no UART command");
    check(LittleFS.files == stored, "all backup GET endpoints leave every file byte unchanged");
    fixtures["/lut/read"] = response("/lut/read", HTTP_POST);
    check(Serial.tx.size() == 8u && Serial.tx[3] == ESP_MSG_LUT_READ,
          "table backup emits exactly one empty LUT_READ frame, not a write or reset");
    const auto read_frame = Serial.tx;
    (void)response("/lut/read", HTTP_POST);
    check(Serial.tx == read_frame, "an already-pending table read does not duplicate UART requests");
    const uint8_t channel1[] = {1,0,2,80,0,0,0,0,0,0,0,100,0,0,0,205,0,0,0};
    const uint8_t channel2[] = {2,0,0};
    func__Esp_StoreLutData(channel1, sizeof channel1);
    func__Esp_StoreLutData(channel2, sizeof channel2);
    fixtures["/lut"] = response("/lut");
    check(BOOL__G__LutReadReady && UINT32_T__G__LutReadMa1[1] == 205u,
          "production table readback retains exact deci-mA values");
    check(LittleFS.files == stored, "table readback does not create, erase or rewrite ESP files");
    check(std::all_of(BOOL__G__TxParamPending, BOOL__G__TxParamPending + ESP_PARAM_COUNT,
                      [](bool pending) { return !pending; }), "backup routes leave all parameter write queues empty");
    UINT8_T__G__LutTxStage = 2u;
    fixtures["lutBusy"] = response("/lut/read", HTTP_POST);
    check(ESP_WEB_SERVER_T__G__Server.lastCode == 409 && Serial.tx == read_frame,
          "a table transfer in progress rejects backup readback without another UART frame");
    UINT8_T__G__LutTxStage = 0u;
    LittleFS.files[ESP_BENCHLOG_PATH].back() ^= 1;
    const auto corrupted = LittleFS.files;
    fixtures["benchDamaged"] = response("/benchlog");
    check(ESP_WEB_SERVER_T__G__Server.lastCode >= 400 && LittleFS.files == corrupted,
          "damaged bench export is refused without silently repairing or deleting stored bytes");
    LittleFS.files = stored;
    LittleFS.files["/evt-20261101.csv"].pop_back();
    const auto damaged_day = LittleFS.files;
    fixtures["dayDamaged"] = response("/evt/dl", HTTP_GET, {{"day", "20261101"}});
    check(ESP_WEB_SERVER_T__G__Server.lastCode == 200 &&
          ESP_WEB_SERVER_T__G__Server.lastBody.find("# WARNING:") != std::string::npos && LittleFS.files == damaged_day,
          "truncated day export emits an in-band warning and preserves original bytes");
    LittleFS.files = stored;
    BOOL__G__FsOk = false;
    fixtures["historyUnmounted"] = response("/evt/days");
    check(ESP_WEB_SERVER_T__G__Server.header("X-Log-Fs") != "1" && LittleFS.files == stored,
          "unmounted history is labeled unavailable rather than a healthy empty archive");
    BOOL__G__FsOk = true;
    UINT8_T__G__LogWriteFault = 0u;
    (void)response("/benchlog/add", HTTP_POST, {{"plain", "# after backup\n"}});
    check(ESP_WEB_SERVER_T__G__Server.lastCode == 200,
          "recording can append after repeated successful and failed backup reads");
    (void)response("/benchlog");
    check(ESP_WEB_SERVER_T__G__Server.lastBody == expanded + "# after backup\n",
          "post-backup recording retains every old measurement and the new append");
    Serial.tx.clear();
    (void)response("/s", HTTP_POST, {{"id", "5"}, {"v", "123"}});
    G_StubMillis += ESP_LINK_TX_INTERVAL_MS;
    func__Esp_PumpTx();
    check(Serial.tx.size() == 13 && Serial.tx[3] == ESP_MSG_SET_PARAM && Serial.tx[6] == 5,
          "normal parameter write still transmits after backup routes have run");
    Serial.tx.clear();
    fixtures["manualExit"] = response("/s", HTTP_POST, {{"id", "19"}, {"v", "0"}});
    G_StubMillis += ESP_LINK_TX_INTERVAL_MS;
    func__Esp_PumpTx();
    check(Serial.tx.size() == 13u && Serial.tx[3] == ESP_MSG_SET_PARAM &&
          Serial.tx[6] == 19u && func__Esp_ReadU32(Serial.tx.data(), 7u) == 0u,
          "explicit manual exit still queues and transmits the safe zero command");
    Serial.tx.clear();
    UINT32_T__G__ParamApplied[19] = 1u;
    G_StubMillis += ESP_LINK_KEEPALIVE_MS;
    UINT32_T__G__LastTlmMs = G_StubMillis;
    UINT32_T__G__LastParamRefreshMs = G_StubMillis;
    (void)response("/t");
    func__Esp_PumpTx();
    check(Serial.tx.size() == 8u && Serial.tx[3] == ESP_MSG_GET_PARAMS,
          "watched manual mode still sends its keepalive after backup activity");
    Serial.tx.clear();
    G_StubMillis += ESP_LINK_BROWSER_LOST_MS + ESP_LINK_KEEPALIVE_MS;
    func__Esp_PumpTx();
    check(Serial.tx.size() == 13u && Serial.tx[3] == ESP_MSG_SET_PARAM &&
          Serial.tx[6] == 19u && func__Esp_ReadU32(Serial.tx.data(), 7u) == 0u,
          "lost-browser protection still requests manual mode off after backup activity");
    std::ofstream output(argv[1]);
    output << '{'; bool first = true;
    for (const auto &entry : fixtures) {
        if (!first) output << ',';
        first = false;
        output << quote(entry.first) << ':' << entry.second;
    }
    output << '}'; output.close();
    check(output.good(), "real handler response fixtures written outside tracked sources");
    std::cout << "ALL " << checks << " BACKUP HOST SAFETY CHECKS PASSED\n";
}
