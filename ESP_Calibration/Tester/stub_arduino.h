/**
 * @file   stub_arduino.h
 * @brief  [EN] Just enough Arduino / ESP8266 surface for the REAL ESP sources
 *              to compile and run on a PC.
 *
 *         The ESP program had no compile check and no test of any kind: the
 *         host syntax script never looked at ESP_Calibration/, so every edit
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
 *         اصلاً به ESP_Calibration/ نگاه نمی‌کرد، پس هر تغییر در پروتکل لینک،
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
#define F(x) (x)
#define SERIAL_8N1 0
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)

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
    String(long v) { char b[24]; snprintf(b, sizeof b, "%ld", v); s = b; }
    String(unsigned int v) { char b[24]; snprintf(b, sizeof b, "%u", v); s = b; }
    const char *c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    int toInt() const { return atoi(s.c_str()); }
    char operator[](size_t i) const { return i < s.size() ? s[i] : '\0'; }
    bool operator==(const char *o) const { return s == (o ? o : ""); }
    String operator+(const String &o) const { return String(s + o.s); }
    String operator+(const char *o) const { return String(s + (o ? o : "")); }
    void trim(void) {
        size_t a = 0, b = s.size();
        while ((a < b) && ((s[a] == ' ') || (s[a] == '\r') || (s[a] == '\n') || (s[a] == '\t'))) a++;
        while ((b > a) && ((s[b - 1] == ' ') || (s[b - 1] == '\r') || (s[b - 1] == '\n') || (s[b - 1] == '\t'))) b--;
        s = s.substr(a, b - a);
    }
    int indexOf(char c) const {
        size_t k = s.find(c);
        return (k == std::string::npos) ? -1 : (int)k;
    }
    int lastIndexOf(char c) const {
        size_t k = s.rfind(c);
        return (k == std::string::npos) ? -1 : (int)k;
    }
    String substring(size_t a) const { return (a >= s.size()) ? String() : String(s.substr(a)); }
    String substring(size_t a, size_t b) const {
        if (a >= s.size() || b <= a) return String();
        if (b > s.size()) b = s.size();
        return String(s.substr(a, b - a));
    }
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
#define WIFI_OFF 0
#define WIFI_AP 1
#define WIFI_NONE_SLEEP 0
struct StubWiFi {
    int modeValue = WIFI_AP;
    int stationNum = 1;
    float outputPowerDbm = 20.5f;
    bool apActive = true;
    void persistent(bool) {}
    void mode(int m) {
        modeValue = m;
        if (m == WIFI_OFF) apActive = false;
    }
    void setSleepMode(int) {}
    void setSleep(bool) {}
    void setOutputPower(float dbm) { outputPowerDbm = dbm; }
    bool softAP(const char *, const char *) { apActive = true; return true; }
    bool softAPdisconnect(bool = false) { apActive = false; return true; }
    int softAPgetStationNum(void) { return stationNum; }
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
    /* [EN] every header of the last answer, so a test can ask for
            X-Log-Fs even when a later sendHeader overwrote the last one.
       [FA] همهٔ سرصفحه‌های آخرین پاسخ، تا تست بتواند X-Log-Fs را بپرسد حتی
            وقتی sendHeader بعدی آخری را نوشته باشد. */
    std::map<std::string, std::string> headers;
    int lastCode = 0;
    std::string lastType, lastBody, lastHeaderKey, lastHeaderVal;
    int sendCount = 0;
    std::string header(const char *k) {
        std::map<std::string, std::string>::iterator it = headers.find(std::string(k));
        return (it == headers.end()) ? std::string() : it->second;
    }

    StubWebServer(int = 80) {}
    void begin(void) {}
    void handleClient(void) {}
    void on(const char *path, handler_t h) { routes[std::make_pair(std::string(path), (int)HTTP_GET)] = h; }
    void on(const char *path, int method, handler_t h) { routes[std::make_pair(std::string(path), method)] = h; }
    void onNotFound(handler_t) {}
    void sendHeader(const char *k, const char *v) {
        lastHeaderKey = k; lastHeaderVal = v; headers[k] = v;
    }
    void setContentLength(size_t) {}
    void send(int code, const char *type, const String &body) {
        lastCode = code; lastType = type ? type : ""; lastBody = body.s; sendCount++;
    }
    void send(int code, const char *type, const char *body) {
        lastCode = code; lastType = type ? type : ""; lastBody = body ? body : ""; sendCount++;
    }
    void send_P(int code, PGM_P type, PGM_P body) {
        lastCode = code; lastType = type ? type : ""; lastBody = body ? body : ""; sendCount++;
    }
    void sendContent_P(PGM_P body, size_t n) {
        if (body && n) lastBody.append(body, n);
        sendCount++;
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
    /* [EN] query argument for the next call, so /clock?sec=.. can be tested.
       [FA] پارامتر پرس‌وجو برای فراخوانی بعدی، تا /clock?sec=.. قابل تست باشد. */
    void addArg(const char *k, const char *v) { args[k] = v; }
    bool has(const char *path, int method) {
        return routes.count(std::make_pair(std::string(path), method)) != 0;
    }
    bool call(const char *path, int method = (int)HTTP_GET) {
        std::map<std::pair<std::string, int>, handler_t>::iterator it =
            routes.find(std::make_pair(std::string(path), method));
        if (it == routes.end()) return false;
        lastCode = 0; lastBody.clear(); headers.clear();
        it->second();
        return true;
    }
};
typedef StubWebServer ESP8266WebServer;

/* ---------------- LittleFS: one in-memory file ---------------------------*/
struct StubFile {
    std::string *buf = nullptr;
    /* [EN] Set by StubFS::failWrites: a real File::print returns the bytes it
            really wrote, and a full flash returns 0. Without this switch no
            host test could ever exercise the path that the user's panel was
            stuck in ("the flash refused the row and nobody noticed").
       [FA] با StubFS::failWrites روشن می‌شود: print روی برد واقعی تعداد بایت
            نوشته‌شده را برمی‌گرداند و فلش پر صفر می‌دهد. بدون این کلید هیچ
            تست هاستی نمی‌توانست همان مسیری را بیازماید که پنل کاربر در آن
            گیر کرده بود («فلش ردیف را نپذیرفت و کسی ندید»). */
    const bool *fail = nullptr;
    size_t pos = 0;
    bool ok = false;
    operator bool() const { return ok; }
    size_t size(void) const { return buf ? buf->size() : 0; }
    bool refuse(void) const { return (fail != nullptr) && (*fail); }
    size_t write(const uint8_t *p, size_t n) {
        if (refuse()) return 0u;
        if (buf) buf->append((const char *)p, n);
        return n;
    }
    size_t print(const char *s) {
        if ((s != NULL) && refuse()) return 0u;
        if ((buf != NULL) && (s != NULL)) buf->append(s);
        return (s != NULL) ? strlen(s) : 0;
    }
    size_t print(const String &v) {       /* Arduino File::print(const String&) */
        if (refuse()) return 0u;
        if (buf != NULL) buf->append(v.s);
        return v.s.size();
    }
    size_t print(unsigned long v) { char b[24]; snprintf(b, sizeof b, "%lu", v); return print(b); }
    size_t print(long v) { char b[24]; snprintf(b, sizeof b, "%ld", v); return print(b); }
    size_t print(unsigned int v) { char b[24]; snprintf(b, sizeof b, "%u", v); return print(b); }
    size_t print(int v) { char b[24]; snprintf(b, sizeof b, "%d", v); return print(b); }
    size_t println(const char *v) {
        if (refuse()) return 0u;
        size_t n = print(v);
        if (buf != NULL) buf->append("\n");
        return n + 1u;
    }
    size_t println(const String &v) {
        if (refuse()) return 0u;
        size_t n = print(v);
        if (buf != NULL) buf->append("\n");
        return n + 1u;
    }
    size_t println(unsigned long v) {
        if (refuse()) return 0u;
        size_t n = print(v);
        if (buf != NULL) buf->append("\n");
        return n + 1u;
    }
    size_t println(void) {
        if (refuse()) return 0u;
        if (buf != NULL) buf->append("\n");
        return 1u;
    }
    int available(void) { return (buf != NULL && pos < buf->size()) ? (int)(buf->size() - pos) : 0; }
    String readString(void) {
        String r;
        if (buf != NULL) r.s = buf->substr(pos);
        pos = (buf != NULL) ? buf->size() : 0u;
        return r;
    }
    String readStringUntil(char terminator) {
        String r;
        if (buf == NULL) return r;
        while (pos < buf->size()) {
            char c = (*buf)[pos++];
            if (c == terminator) break;
            r.s.push_back(c);
        }
        return r;
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
    /* [EN] true = every write is refused, exactly like a full flash.
       [FA] روشن = هر نوشتن رد می‌شود، درست مثل فلش پر. */
    bool failWrites = false;
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
        f.fail = &failWrites;
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
