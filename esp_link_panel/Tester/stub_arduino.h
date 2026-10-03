/**
 * @file   stub_arduino.h
 * @brief  [EN] Just enough Arduino / ESP8266 surface for the REAL ESP sources
 *              to compile and run on a PC.
 *
 *         The ESP program had no compile check and no test of any kind: the
 *         host syntax script never looked at esp_link_panel/, so every edit
 *         to the link protocol, the HTTP handlers or the parameter table was
 *         carried straight to a board and found there, by hand. These stubs
 *         close that gap. They are deliberately thin - they implement the
 *         behaviour the firmware actually depends on and nothing else, so a
 *         test passing here means the firmware's own logic is right, not
 *         that a clever fake agreed with it.
 *
 *         What is faked: Serial (a byte queue both ways), the web server
 *         (records what a handler sent, plays back query arguments),
 *         LittleFS (an in-memory file), WiFi, millis and String. What is
 *         real: every line of plink_config.h, plink_params.h, plink_state.h,
 *         plink_link.h and plink_http.h, compiled exactly as the board
 *         compiles them.
 *
 * @brief  [FA] کمینهٔ لازم از Arduino / ESP8266 تا سورس واقعی ESP روی PC
 *              کامپایل و اجرا شود.
 *
 *         برنامهٔ ESP نه چک کامپایل داشت نه هیچ تستی: اسکریپت syntax هاست
 *         اصلاً به esp_link_panel/ نگاه نمی‌کرد، پس هر تغییر در پروتکل لینک،
 *         هندلرهای HTTP یا جدول پارامترها مستقیم روی برد می‌رفت و همان‌جا
 *         دستی پیدا می‌شد. این استاب‌ها همین شکاف را می‌بندند. عمداً نازک‌اند:
 *         فقط رفتاری را پیاده می‌کنند که فرم‌ور واقعاً به آن تکیه دارد، تا
 *         قبول‌شدن یک تست یعنی منطق خود فرم‌ور درست است، نه اینکه یک تقلبِ
 *         زیرک با آن موافقت کرده.
 */
#ifndef STUB_ARDUINO_H
#define STUB_ARDUINO_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <deque>
#include <map>

/* ---------------- board selection: the tests target the ESP8266 path ----- */
#ifndef ESP8266
#define ESP8266 1
#endif

/* ---------------- PROGMEM is plain memory on a PC ----------------------- */
#define PROGMEM
#define PGM_P const char *
#define pgm_read_byte(a) (*(const uint8_t *)(a))
#define strlen_P strlen
#define memcpy_P memcpy
#define snprintf_P snprintf
#define F(x) (x)
#define SERIAL_8N1 0

/* ---------------- a monotonic clock the test drives --------------------- */
extern unsigned long G_StubMillis;
inline unsigned long millis(void) { return G_StubMillis; }
inline void delay(unsigned long ms) { G_StubMillis += ms; }
inline void yield(void) {}

/* ---------------- String: only what the firmware uses ------------------- */
class String {
public:
    std::string s;
    String() {}
    String(const char *p) : s(p ? p : "") {}
    String(const std::string &v) : s(v) {}
    String(int v) { char b[24]; snprintf(b, sizeof b, "%d", v); s = b; }
    String(unsigned long v) { char b[24]; snprintf(b, sizeof b, "%lu", v); s = b; }
    const char *c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    int toInt() const { return atoi(s.c_str()); }
    char operator[](size_t i) const { return i < s.size() ? s[i] : '\0'; }
    bool operator==(const char *o) const { return s == (o ? o : ""); }
    String operator+(const String &o) const { return String(s + o.s); }
};

/* ---------------- Serial: a queue in each direction ---------------------
   [EN] rx is what the STM32 "sent" to the ESP, tx is what the ESP wrote
        back. A test fills rx, pumps the firmware, and reads tx.
   [FA] rx چیزی است که STM32 به ESP «فرستاده» و tx چیزی که ESP نوشته. */
struct StubSerial {
    std::deque<uint8_t> rx;
    std::vector<uint8_t> tx;
    void setRxBufferSize(size_t) {}
    void begin(unsigned long, int = 0) {}
    int available(void) { return (int)rx.size(); }
    int read(void) {
        if (rx.empty()) return -1;
        int c = rx.front();
        rx.pop_front();
        return c;
    }
    size_t write(const uint8_t *p, size_t n) {
        tx.insert(tx.end(), p, p + n);
        return n;
    }
    size_t write(uint8_t b) { tx.push_back(b); return 1; }
    void print(const char *) {}
    void println(const char *) {}
    void flush(void) {}
    void feed(const std::vector<uint8_t> &v) { rx.insert(rx.end(), v.begin(), v.end()); }
};
extern StubSerial Serial;

/* ---------------- WiFi ---------------------------------------------------*/
#define WIFI_AP 1
#define WIFI_NONE_SLEEP 0
struct StubWiFi {
    void mode(int) {}
    void setSleepMode(int) {}
    void setSleep(bool) {}
    bool softAP(const char *, const char *) { return true; }
    int softAPgetStationNum(void) { return 1; }
};
extern StubWiFi WiFi;

/* ---------------- web server: records the last response ------------------
   [EN] Handlers are registered by path and can be invoked by a test; args
        are fed in beforehand, and whatever the handler sends is captured.
   [FA] هندلرها با مسیر ثبت می‌شوند و تست می‌تواند صدایشان بزند. */
#define HTTP_GET 1
#define HTTP_POST 2
struct StubFile;

struct StubWebServer {
    typedef void (*handler_t)(void);
    std::map<std::pair<std::string, int>, handler_t> routes;
    std::map<std::string, std::string> args;
    int lastCode = 0;
    std::string lastType, lastBody, lastHeaderKey, lastHeaderVal;
    int sendCount = 0;

    StubWebServer(int = 80) {}
    void begin(void) {}
    void handleClient(void) {}
    void on(const char *path, handler_t h) { routes[std::make_pair(std::string(path), (int)HTTP_GET)] = h; }
    void on(const char *path, int method, handler_t h) { routes[std::make_pair(std::string(path), method)] = h; }
    void onNotFound(handler_t) {}
    void sendHeader(const char *k, const char *v) { lastHeaderKey = k; lastHeaderVal = v; }
    void send(int code, const char *type, const String &body) {
        lastCode = code; lastType = type ? type : ""; lastBody = body.s; sendCount++;
    }
    void send(int code, const char *type, const char *body) {
        lastCode = code; lastType = type ? type : ""; lastBody = body ? body : ""; sendCount++;
    }
    void send_P(int code, PGM_P type, PGM_P body) {
        lastCode = code; lastType = type ? type : ""; lastBody = body ? body : ""; sendCount++;
    }
    bool hasArg(const char *n) { return args.count(n) != 0; }
    String arg(const char *n) {
        std::map<std::string, std::string>::iterator it = args.find(n);
        return String(it == args.end() ? std::string() : it->second);
    }
    size_t streamFile(StubFile &f, const char *type);
    String uri(void) { return String("/"); }
    /* test helpers */
    void setArg(const char *n, const char *v) { args[n] = v; }
    void clearArgs(void) { args.clear(); }
    bool has(const char *path, int method) {
        return routes.count(std::make_pair(std::string(path), method)) != 0;
    }
    bool call(const char *path, int method = (int)HTTP_GET) {
        std::map<std::pair<std::string, int>, handler_t>::iterator it =
            routes.find(std::make_pair(std::string(path), method));
        if (it == routes.end()) return false;
        lastCode = 0; lastBody.clear();
        it->second();
        return true;
    }
};
typedef StubWebServer ESP8266WebServer;

/* ---------------- LittleFS: one in-memory file ---------------------------*/
struct StubFile {
    std::string *buf = nullptr;
    size_t pos = 0;
    bool ok = false;
    operator bool() const { return ok; }
    size_t size(void) const { return buf ? buf->size() : 0; }
    size_t write(const uint8_t *p, size_t n) {
        if (buf) buf->append((const char *)p, n);
        return n;
    }
    size_t print(const char *s) {
        if ((buf != NULL) && (s != NULL)) buf->append(s);
        return (s != NULL) ? strlen(s) : 0;
    }
    size_t print(const String &v) {       /* Arduino File::print(const String&) */
        if (buf != NULL) buf->append(v.s);
        return v.s.size();
    }
    int read(void) {
        if (!buf || pos >= buf->size()) return -1;
        return (uint8_t)(*buf)[pos++];
    }
    size_t readBytes(char *d, size_t n) {
        if (!buf) return 0;
        size_t k = 0;
        while (k < n && pos < buf->size()) d[k++] = (*buf)[pos++];
        return k;
    }
    bool seek(size_t p) { pos = p; return buf && p <= buf->size(); }
    void close(void) {}
};
typedef StubFile File;

struct StubFS {
    std::map<std::string, std::string> files;
    bool mounted = false;
    bool begin(bool = false) { mounted = true; return true; }
    bool format(void) { files.clear(); return true; }
    bool exists(const char *p) { return files.count(p) != 0; }
    bool remove(const char *p) { return files.erase(p) > 0; }
    StubFile open(const char *p, const char *mode) {
        StubFile f;
        std::string key(p);
        if (mode && mode[0] == 'r') {
            if (!files.count(key)) return f;
        } else if (mode && mode[0] == 'w') {
            files[key] = "";
        } else if (!files.count(key)) {
            files[key] = "";
        }
        f.buf = &files[key];
        f.ok = true;
        return f;
    }
};
inline size_t StubWebServer::streamFile(StubFile &f, const char *type)
{
    lastCode = 200;
    lastType = (type != NULL) ? type : "";
    lastBody = (f.buf != NULL) ? *f.buf : std::string();
    sendCount++;
    return lastBody.size();
}

extern StubFS LittleFS;

#endif /* STUB_ARDUINO_H */
