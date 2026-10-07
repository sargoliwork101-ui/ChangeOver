/**
 * @file    stub_esp.h
 * @brief   [EN] Just enough Arduino / ESP8266 surface for the USER PANEL to
 *              compile and run on a PC, and to be tested by hand.
 *
 *          WHY THIS EXISTS
 *            Nothing compiles user_panel/ except the Arduino IDE on a machine
 *            that has the ESP8266 core installed. That means a typo, a missing
 *            cast or a C++ keyword used in a C way is found by flashing a board
 *            - the slowest possible place to find it. This file fakes the small
 *            part of the platform the panel actually touches (Serial, WiFi,
 *            WiFiClient, LittleFS, the web server, String, millis), so
 *            tools/check_user_panel.sh can compile the REAL .ino and the real
 *            headers with -Wall -Wextra -Werror and then run tests against
 *            them - including the sanitizers, which see a one-past-the-end
 *            write that an assertion cannot.
 *
 *          WHAT IS FAKED, WHAT IS REAL
 *            Faked: the radio, the file system, the HTTP socket. Real: every
 *            line of up_config.h, up_state.h, up_store.h, up_auth.h,
 *            up_sha256.h, up_history.h, up_link.h and up_http.h, compiled
 *            exactly as the board compiles them, in the same single translation
 *            unit, in the same include order.
 *
 *          HOW THE WEB SERVER FAKE WORKS
 *            Routes are registered by path and method exactly as on the board.
 *            A test sets the request arguments and the request headers
 *            (the session cookie lives there), calls the route, and then reads
 *            whatever the handler sent: status, content type, body, and the
 *            headers it wanted. That is enough to test login, roles, the admin
 *            actions and the CSV export end to end without a browser.
 *
 * @brief   [FA] کمینهٔ لازم از Arduino / ESP8266 تا پنل کاربر روی PC کامپایل و
 *              دستی تست شود.
 *
 *          چرا این فایل وجود دارد
 *            هیچ‌چیز user_panel/ را کامپایل نمی‌کند جز Arduino IDE روی دستگاهی
 *            که هستهٔ ESP8266 دارد. یعنی یک غلط تایپی، یک cast جاافتاده یا یک
 *            کلیدواژهٔ C که به‌سبک C به کار رفته، سر فلش‌کردن برد پیدا می‌شود -
 *            کندترین جای ممکن. این فایل همان بخش کوچکی از پلتفرم را که پنل
 *            واقعاً لمس می‌کند جعل می‌کند (Serial، WiFi، WiFiClient، LittleFS،
 *            سرور وب، String، millis) تا tools/check_user_panel.sh بتواند
 *            خودِ .ino و هدرهای واقعی را با -Wall -Wextra -Werror کامپایل کند
 *            و بعد تست‌هایی رویشان اجرا کند - از جمله سنیتایزرها که نوشتن یک
 *            خانه بعد از انتها را می‌بینند و assert نمی‌بیند.
 *
 *          چه چیزی جعل شده و چه چیزی واقعی است
 *            جعل‌شده: رادیو، فایل‌سیستم، سوکت HTTP. واقعی: هر خط از
 *            up_config.h، up_state.h، up_store.h، up_auth.h، up_sha256.h،
 *            up_history.h، up_link.h و up_http.h، دقیقاً همان‌طور که برد
 *            کامپایل می‌کند، در همان یک واحد ترجمه و با همان ترتیب include.
 *
 *          سرور وب جعلی چطور کار می‌کند
 *            مسیرها با نام و متد ثبت می‌شوند، عیناً مثل برد. تست آرگومان‌های
 *            درخواست و سرصفحه‌های آن (کوکی نشست همان‌جاست) را می‌گذارد، مسیر را
 *            صدا می‌زند و بعد هرچه هندلر فرستاده را می‌خواند: کد، نوع محتوا،
 *            بدنه و سرصفحه‌هایی که خواسته. همین برای تست ورود، نقش‌ها،
 *            اقدامات مدیر و خروجی CSV بدون مرورگر کافی است.
 */

#ifndef STUB_ESP_H
#define STUB_ESP_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <utility>

/* ---------------- board selection: this sketch targets the ESP8266 path -- */
#ifndef ESP8266
#define ESP8266 1
#endif

/* ---------------- PROGMEM is plain memory on a PC ----------------------- */
#define PROGMEM
#define PGM_P const char *
#define pgm_read_byte(address) (*(const uint8_t *)(address))
#define pgm_read_word(address) (*(const uint16_t *)(address))
#define strlen_P strlen
#define strncpy_P strncpy
#define memcpy_P memcpy
#define snprintf_P snprintf
#define F(literal) (literal)

/* ---------------- GPIO -------------------------------------------------- */
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

/* ---------------- a monotonic clock the test drives --------------------- */
inline unsigned long &stub_millis_ref(void)
{
    static unsigned long g_stub_millis = 0u;
    return g_stub_millis;
}
inline unsigned long &stub_micros_ref(void)
{
    static unsigned long g_stub_micros = 0u;
    return g_stub_micros;
}
inline unsigned long millis(void) { return stub_millis_ref(); }
inline unsigned long micros(void) { return stub_micros_ref(); }
inline void delay(unsigned long ms)
{
    stub_millis_ref() += ms;
    stub_micros_ref() += (ms * 1000u);
}
inline void yield(void) {}

extern std::map<uint8_t, int> G_StubGpio;   /* [EN] pin - level / [FA] پایه - سطح */
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t pin, uint8_t level) { G_StubGpio[pin] = (int)level; }

/* ---------------- String: only what the panel uses ---------------------- */
class String {
public:
    std::string value;

    String() {}
    String(const char *text) : value(text != NULL ? text : "") {}
    String(const std::string &text) : value(text) {}
    String(char c) : value(1u, c) {}
    String(int number) { char b[24]; snprintf(b, sizeof b, "%d", number); value = b; }
    String(unsigned int number) { char b[24]; snprintf(b, sizeof b, "%u", number); value = b; }
    String(long number) { char b[24]; snprintf(b, sizeof b, "%ld", number); value = b; }
    String(unsigned long number) { char b[24]; snprintf(b, sizeof b, "%lu", number); value = b; }

    const char *c_str(void) const { return value.c_str(); }
    size_t length(void) const { return value.size(); }
    int toInt(void) const { return atoi(value.c_str()); }
    char operator[](size_t index) const { return (index < value.size()) ? value[index] : '\0'; }

    bool operator==(const char *other) const { return value == std::string(other != NULL ? other : ""); }
    bool operator==(const String &other) const { return value == other.value; }
    bool operator!=(const char *other) const { return !(*this == other); }
    bool operator!=(const String &other) const { return value != other.value; }

    String operator+(const String &other) const { return String(value + other.value); }
    String operator+(const char *other) const { return String(value + std::string(other != NULL ? other : "")); }
    String &operator+=(const String &other) { value += other.value; return *this; }
};

/* ---------------- Serial: a line log ----------------------------------- */
class IPAddress;   /* [EN] the print overloads live below its definition / [FA] پایین‌تر تعریف می‌شود */

struct StubSerial {
    std::vector<std::string> lines;

    void begin(unsigned long, int = 0) {}
    void print(const char *text) { if (text != NULL) { lines.push_back(std::string(text)); } }
    void print(const String &text) { lines.push_back(text.value); }
    void print(int number) { char b[24]; snprintf(b, sizeof b, "%d", number); lines.push_back(b); }
    void print(unsigned int number) { char b[24]; snprintf(b, sizeof b, "%u", number); lines.push_back(b); }
    void print(unsigned long number) { char b[24]; snprintf(b, sizeof b, "%lu", number); lines.push_back(b); }
    void println(const char *text) { lines.push_back(std::string(text != NULL ? text : "") + "\n"); }
    void print(const IPAddress &address);         /* [EN] defined below IPAddress */
    void println(const IPAddress &address);       /* [FA] پایین‌تر تعریف می‌شود */
    void println(const String &text) { lines.push_back(text.value + "\n"); }
    void println(unsigned int number) { print(number); lines.push_back("\n"); }
    void println(unsigned long number) { print(number); lines.push_back("\n"); }
    void println(void) { lines.push_back("\n"); }
    void flush(void) {}
    std::string all(void) const
    {
        std::string out;
        for (size_t i = 0u; i < lines.size(); i++) { out += lines[i]; }
        return out;
    }
};
extern StubSerial Serial;

/* ---------------- IPAddress ------------------------------------------- */
class IPAddress {
public:
    uint8_t octets[4];
    IPAddress() { octets[0] = octets[1] = octets[2] = octets[3] = 0u; }
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { octets[0] = a; octets[1] = b; octets[2] = c; octets[3] = d; }
    std::string toString(void) const
    {
        char b[20];
        snprintf(b, sizeof b, "%u.%u.%u.%u", (unsigned)octets[0], (unsigned)octets[1], (unsigned)octets[2], (unsigned)octets[3]);
        return std::string(b);
    }
    bool operator==(const IPAddress &other) const
    {
        return (memcmp(octets, other.octets, sizeof(octets)) == 0);
    }
};

inline void StubSerial::print(const IPAddress &address) { lines.push_back(address.toString()); }
inline void StubSerial::println(const IPAddress &address) { println(address.toString().c_str()); }

/* ---------------- WiFi ------------------------------------------------- */
#define WIFI_AP 2
#define WIFI_STA 1
#define WIFI_AP_STA 3
#define WIFI_NONE_SLEEP 0
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6

struct StubWiFi {
    int currentStatus = WL_DISCONNECTED;
    IPAddress apIp;
    IPAddress staIp{192, 168, 4, 50};
    bool apConfigured = false;
    bool apStarted = false;
    bool staStarted = false;
    std::string lastSsid;
    std::string lastPass;
    /* [EN] What the STATION was told to join, kept apart from lastSsid because
            softAP() writes that one too: a panel that offers its own network and
            joins somebody else's must not confuse the two.
       [FA] آنچه به کلاینت گفته شد بپیوندد، جدا از lastSsid چون softAP() آن یکی
            را هم می‌نویسد: پنلی که شبکهٔ خودش را عرضه می‌کند و به شبکهٔ دیگری
            می‌پیوندد نباید این دو را قاطی کند. */
    std::string staSsid;
    std::string staPass;

    void mode(int) {}
    void setSleepMode(int) {}
    void setSleep(bool) {}
    bool softAPConfig(IPAddress local, IPAddress, IPAddress)
    {
        apIp = local;
        apConfigured = true;
        return true;
    }
    bool softAP(const char *ssid, const char *pass, int = 1)
    {
        lastSsid = (ssid != NULL) ? ssid : "";
        lastPass = (pass != NULL) ? pass : "";
        apStarted = true;
        return true;
    }
    bool softAP(const char *ssid, const char *pass) { return softAP(ssid, pass, 1); }
    void begin(const char *ssid, const char *pass)
    {
        lastSsid = (ssid != NULL) ? ssid : "";
        lastPass = (pass != NULL) ? pass : "";
        staSsid = lastSsid;
        staPass = lastPass;
        staStarted = true;
    }
    void disconnect(void) { currentStatus = WL_DISCONNECTED; }
    int status(void) { return currentStatus; }
    /* [EN] On the real ESP8266 localIP() is the STATION address (this panel on
            the board's network) and softAPIP() is the panel's own AP. Keeping
            that distinction here matters: the network card shows the address the
            panel got from the board, which is how the admin knows the join
            actually worked.
       [FA] روی ESP8266 واقعی localIP() آدرس کلاینت است (این پنل روی شبکهٔ برد) و
            softAPIP() آدرس اکسس‌پوینت خود پنل. نگه‌داشتن این تفاوت اینجا مهم
            است: کارت شبکه همان آدرسی را نشان می‌دهد که پنل از برد گرفته، و مدیر
            از همان می‌فهمد پیوستن واقعاً انجام شده. */
    IPAddress localIP(void) { return (currentStatus == WL_CONNECTED) ? staIp : IPAddress(0u, 0u, 0u, 0u); }
    IPAddress softAPIP(void) { return apIp; }
    int softAPgetStationNum(void) { return 1; }
    std::string SSID(void) const { return lastSsid; }
    int RSSI(void) { return -60; }
    std::string macAddress(void) const { return std::string("AA:BB:CC:DD:EE:FF"); }
    bool hostname(const char *) { return true; }
    bool persistent(bool) { return true; }
    bool setAutoConnect(bool) { return true; }
};
extern StubWiFi WiFi;

/* ---------------- WiFiClient: a scripted speech bubble ------------------
   [EN] A test loads the next answer from the board ("HTTP/1.1 200 OK..."),
        the link code connects, writes its request (which is captured so a test
        can assert the POST body that would reach the board) and reads the
        answer back.
   [FA] تست پاسخ بعدی برد را بار می‌گذارد؛ کد لینک وصل می‌شود، درخواستش را
        می‌نویسد (که ضبط می‌شود تا تست بتواند بدنهٔ POST رسیده به برد را
        تأیید کند) و پاسخ را می‌خواند. */
struct WiFiClient {
    static std::string G_NextResponse;      /* [EN] what the board will say / [FA] پاسخ برد */
    static std::string G_LastRequest;       /* [EN] what we sent / [FA] آنچه فرستادیم */
    static int G_ConnectFails;              /* [EN] consume N connects / [FA] چند اتصال ناموفق */

    size_t readPos = 0u;
    bool open = false;
    IPAddress remoteIp;
    bool hasIp = false;

    bool connect(const char *host, uint16_t port)
    {
        if (G_ConnectFails > 0) { G_ConnectFails--; open = false; return false; }
        (void)host;
        (void)port;
        readPos = 0u;
        open = true;
        return true;
    }
    bool connect(IPAddress ip, uint16_t port) { remoteIp = ip; hasIp = true; return connect("ip", port); }
    size_t print(const char *text) { if (text != NULL) { G_LastRequest += text; } return (text != NULL) ? strlen(text) : 0u; }
    size_t print(const String &text) { G_LastRequest += text.value; return text.value.size(); }
    size_t println(const char *text) { print(text); G_LastRequest += "\r\n"; return 2u; }
    int available(void) { return open ? (int)(G_NextResponse.size() - readPos) : 0; }
    bool connected(void) { return open && (readPos < G_NextResponse.size()); }
    int read(void)
    {
        if (!connected()) { return -1; }
        return (uint8_t)G_NextResponse[readPos++];
    }
    int read(uint8_t *buffer, size_t want)
    {
        size_t got = 0u;
        while ((got < want) && connected()) { buffer[got] = (uint8_t)G_NextResponse[readPos++]; got++; }
        return (int)got;
    }
    void setTimeout(unsigned long) {}
    void setNoDelay(bool) {}
    void stop(void) { open = false; }
};

/* ---------------- LittleFS -------------------------------------------- */
#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

struct StubFile {
    std::string *buffer = NULL;
    size_t pos = 0u;
    bool valid = false;
    std::string path;

    operator bool(void) const { return valid; }
    size_t size(void) const { return (buffer != NULL) ? buffer->size() : 0u; }
    size_t position(void) const { return pos; }
    int available(void) const { return (buffer != NULL) ? (int)(buffer->size() - pos) : 0; }
    int read(void)
    {
        if ((buffer == NULL) || (pos >= buffer->size())) { return -1; }
        return (uint8_t)(*buffer)[pos++];
    }
    size_t read(uint8_t *out, size_t want)
    {
        if (buffer == NULL) { return 0u; }
        size_t got = 0u;
        while ((got < want) && (pos < buffer->size())) { out[got] = (uint8_t)(*buffer)[pos]; got++; pos++; }
        return got;
    }
    size_t write(const uint8_t *data, size_t count)
    {
        if (buffer == NULL) { return 0u; }
        if (pos > buffer->size()) { buffer->resize(pos, '\0'); }
        for (size_t i = 0u; i < count; i++)
        {
            if (pos < buffer->size()) { (*buffer)[pos] = (char)data[i]; }
            else { buffer->push_back((char)data[i]); }
            pos++;
        }
        return count;
    }
    size_t write(uint8_t data) { return write(&data, 1u); }
    size_t print(const char *text) { return (text != NULL) ? write((const uint8_t *)text, strlen(text)) : 0u; }
    size_t print(const String &text) { return write((const uint8_t *)text.value.data(), text.value.size()); }
    bool seek(size_t where)
    {
        if ((buffer == NULL) || (where > buffer->size())) { return false; }
        pos = where;
        return true;
    }
    void flush(void) {}
    void close(void) { valid = false; }
    std::string name(void) const { return path; }
    bool isDirectory(void) const { return false; }
};

typedef StubFile File;   /* [EN] the Arduino name / [FA] نام آردوینویی */

struct StubFS {
    std::map<std::string, std::string> files;
    size_t capacity = 262144u;      /* [EN] 256 KB "flash" / [FA] فلش ۲۵۶ کیلوبایتی */
    bool mounted = false;
    bool mountFails = false;

    bool begin(bool = false)
    {
        if (mountFails) { mounted = false; return false; }
        mounted = true;
        return true;
    }
    bool format(void) { files.clear(); return true; }
    bool exists(const char *path) { return files.count(std::string(path != NULL ? path : "")) != 0; }
    bool remove(const char *path) { return files.erase(std::string(path != NULL ? path : "")) > 0; }
    bool rename(const char *from, const char *to)
    {
        std::string a(from != NULL ? from : "");
        std::string b(to != NULL ? to : "");
        if (files.count(a) == 0) { return false; }
        files[b] = files[a];
        files.erase(a);
        return true;
    }
    size_t totalBytes(void) { return capacity; }
    size_t usedBytes(void)
    {
        size_t total = 0u;
        for (std::map<std::string, std::string>::iterator it = files.begin(); it != files.end(); ++it) { total += it->second.size(); }
        return total;
    }
    StubFile open(const char *path, const char *mode)
    {
        StubFile handle;
        std::string key(path != NULL ? path : "");
        char first = (mode != NULL) ? mode[0] : 'r';

        handle.path = key;
        if (!mounted) { return handle; }

        if (first == 'r')
        {
            if (files.count(key) == 0) { return handle; }
            handle.pos = 0u;
        }
        else if (first == 'w')
        {
            files[key] = std::string();
            handle.pos = 0u;
        }
        else if (first == 'a')
        {
            if (files.count(key) == 0) { files[key] = std::string(); }
            handle.pos = files[key].size();
        }
        else
        {
            if (files.count(key) == 0) { files[key] = std::string(); }
            handle.pos = 0u;
        }

        handle.buffer = &files[key];
        handle.valid = true;
        return handle;
    }
};
extern StubFS LittleFS;

/* ---------------- web server ------------------------------------------ */
#define HTTP_GET 1
#define HTTP_POST 2
#define HTTP_ANY 3
#define CONTENT_LENGTH_UNKNOWN ((size_t)0xFFFFFFFFu)

struct StubWebServer {
    typedef void (*handler_t)(void);

    std::map<std::pair<std::string, int>, handler_t> routes;
    std::map<std::string, std::string> args;
    std::map<std::string, std::string> requestHeaders;
    std::map<std::string, std::string> collectedHeaderKeys;

    int lastCode = 0;
    std::string lastType;
    std::string lastBody;
    std::string lastSentHeaderKey;
    std::string lastSentHeaderValue;
    std::map<std::string, std::string> sentHeaders;
    size_t declaredLength = 0u;
    int sendCount = 0;

    StubWebServer(int = 80) {}

    void begin(void) {}
    void handleClient(void) {}
    void on(const char *path, handler_t handler) { on(path, HTTP_GET, handler); }
    void on(const char *path, int method, handler_t handler)
    {
        routes[std::make_pair(std::string(path), method)] = handler;
    }
    void onNotFound(handler_t) {}
    void collectHeaders(const char *const keys[], size_t count)
    {
        collectedHeaderKeys.clear();
        for (size_t i = 0u; i < count; i++) { collectedHeaderKeys[keys[i]] = keys[i]; }
    }
    bool hasHeader(const char *name) { return requestHeaders.count(std::string(name)) != 0; }
    String header(const char *name)
    {
        std::map<std::string, std::string>::iterator it = requestHeaders.find(std::string(name));
        return String((it == requestHeaders.end()) ? std::string() : it->second);
    }
    void sendHeader(const char *key, const char *value)
    {
        lastSentHeaderKey = (key != NULL) ? key : "";
        lastSentHeaderValue = (value != NULL) ? value : "";
        sentHeaders[lastSentHeaderKey] = lastSentHeaderValue;
    }
    void setContentLength(size_t length) { declaredLength = length; }
    void send(int code, const char *type, const char *body)
    {
        lastCode = code;
        lastType = (type != NULL) ? type : "";
        lastBody = (body != NULL) ? body : "";
        sendCount++;
    }
    void send(int code, const char *type, const String &body)
    {
        lastCode = code;
        lastType = (type != NULL) ? type : "";
        lastBody = body.value;
        sendCount++;
    }
    void send_P(int code, PGM_P type, PGM_P body)
    {
        lastCode = code;
        lastType = (type != NULL) ? type : "";
        lastBody = (body != NULL) ? body : "";
        sendCount++;
    }
    size_t sendContent(const char *chunk)
    {
        if (chunk != NULL) { lastBody += chunk; }
        return (chunk != NULL) ? strlen(chunk) : 0u;
    }
    /* [EN] The binary-safe form, exactly like the real ESP8266 core's
       sendContent(const char*, size_t): the Excel report is a zip and its bytes
       contain NULs, so the length-taking overload is the only one that can carry
       it. It also lets a test compare the whole workbook byte for byte.
       [FA] همان شکل ایمن برای دادهٔ دودویی که در هستهٔ واقعی ESP8266 هم وجود
       دارد: گزارش اکسل یک زیپ است و بایت‌هایش صفر دارند، پس تنها همین شکل
       طول‌دار می‌تواند حملش کند. همچنین به تست اجازه می‌دهد کل کتاب را بایت‌به‌بایت
       مقایسه کند. */
    size_t sendContent(const char *chunk, size_t length)
    {
        if (chunk != NULL) { lastBody.append(chunk, length); }
        return length;
    }
    size_t sendContent(const String &chunk) { lastBody += chunk.value; return chunk.value.size(); }
    size_t sendContent_P(PGM_P chunk)
    {
        lastBody += (chunk != NULL) ? chunk : "";
        return lastBody.size();
    }
    bool hasArg(const char *name) { return args.count(std::string(name)) != 0; }
    String arg(const char *name)
    {
        std::map<std::string, std::string>::iterator it = args.find(std::string(name));
        return String((it == args.end()) ? std::string() : it->second);
    }
    String uri(void) { return String("/"); }
    int method(void) { return HTTP_GET; }
    size_t streamFile(StubFile &file, const char *type)
    {
        lastCode = 200;
        lastType = (type != NULL) ? type : "";
        lastBody = (file.buffer != NULL) ? *file.buffer : std::string();
        sendCount++;
        return lastBody.size();
    }

    /* ---- test helpers / کمکی‌های تست ---- */
    void setArg(const char *name, const char *value) { args[std::string(name)] = std::string(value != NULL ? value : ""); }
    void setHeader(const char *name, const char *value) { requestHeaders[std::string(name)] = std::string(value != NULL ? value : ""); }
    void clearArgs(void) { args.clear(); }
    void clearHeaders(void) { requestHeaders.clear(); }
    void clearResponse(void) { lastCode = 0; lastType.clear(); lastBody.clear(); sendCount = 0; sentHeaders.clear(); declaredLength = 0u; }
    bool has(const char *path, int method) { return routes.count(std::make_pair(std::string(path), method)) != 0; }
    bool call(const char *path, int method = HTTP_GET)
    {
        std::map<std::pair<std::string, int>, handler_t>::iterator it =
            routes.find(std::make_pair(std::string(path), method));
        if (it == routes.end()) { return false; }
        clearResponse();
        it->second();
        return true;
    }
};
typedef StubWebServer ESP8266WebServer;

#endif /* STUB_ESP_H */
