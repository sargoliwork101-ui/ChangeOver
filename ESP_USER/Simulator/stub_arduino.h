/* stub_arduino.h - کمینهٔ لازم از Arduino/ESP8266 تا برنامهٔ واقعی پنل ساده روی PC اجرا شود
   فقط فارسی - مثل بقیهٔ این پوشه

   چه چیزی جعلی است: Serial (صف بایت دوطرفه)، LittleFS (پوشه‌ای روی دیسک)،
   WiFi، ESP.restart، millis و String.
   چه چیزی واقعی است: تمام `ESP_USER.ino` و `simple_config.h` و
   `simple_state.h` و `simple_panel.h` و `simple_link.h` و `simple_auth.h` و
   `simple_diag.h` و `simple_http.h` و `simple_theme.h` - همان‌طور که برد
   کامپایل می‌کند.

   این استاب‌ها عمداً نازک‌اند: فقط رفتاری را پیاده می‌کنند که خود برنامه به آن
   تکیه دارد، تا هر چه در سیمولاتور دیده می‌شود نتیجهٔ منطق خود برنامه باشد،
   نه توافق یک تقلبی زیرک با آن.

   نام‌گذاری: متغیرهای محلی `type__name` و سراسری‌ها `TYPE__G__NAME` هستند،
   مطابق قانون پنل. تنها استثنا نام‌های خود Arduino/ESP8266 است (`String`،
   `File`، `Serial`، `millis`، `PROGMEM`، ...) چون خود برنامه همین‌ها را صدا
   می‌زند و عوض‌کردنشان یعنی دست‌بردن در برنامهٔ فلش‌شده.
*/
#ifndef SIM_STUB_ARDUINO_H
#define SIM_STUB_ARDUINO_H

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iterator>
#include <string>
#include <vector>

#include <sys/time.h>
#include <unistd.h>

/* ==================== انتخاب برد: شاخهٔ ESP8266 ==================== */
#ifndef ESP8266
#define ESP8266 1
#endif

/* ==================== ماکروهای کتابخانه روی PC ==================== */
/* روی PC، PROGMEM و F فقط حافظهٔ معمولی هستند و SERIAL_8N1 عدد بی‌اثری است */
#define PROGMEM
#define PGM_P const char *
#define F(text) (text)
#define SERIAL_8N1 0
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)

#define WIFI_OFF 0
#define WIFI_AP 2
#define WIFI_NONE_SLEEP 0

/* ==================== ساعت یکنوا ==================== */
static unsigned long UNSIGNED_LONG__G__MillisStart = 0u;

inline unsigned long millis(void)
{
    struct timeval timeval__now;
    gettimeofday(&timeval__now, NULL);

    unsigned long unsigned_long__absolute =
        (unsigned long)((timeval__now.tv_sec * 1000L) + (timeval__now.tv_usec / 1000L));

    if (UNSIGNED_LONG__G__MillisStart == 0u)
    {
        UNSIGNED_LONG__G__MillisStart = unsigned_long__absolute;
    }
    return unsigned_long__absolute - UNSIGNED_LONG__G__MillisStart;
}

/* میکروثانیه: همان ساعت میلی‌ثانیه با دقت بیشتر (برنامه برای یکتاشدن
   شمارندهٔ رمزنگاری از micros استفاده می‌کند) */
inline unsigned long micros(void)
{
    struct timeval timeval__now;
    gettimeofday(&timeval__now, NULL);
    unsigned long unsigned_long__absolute =
        (unsigned long)((timeval__now.tv_sec * 1000000L) + timeval__now.tv_usec);
    if (UNSIGNED_LONG__G__MillisStart == 0u)
    {
        (void)millis();
    }
    return unsigned_long__absolute - (UNSIGNED_LONG__G__MillisStart * 1000uL);
}

inline void delay(unsigned long unsigned_long__ms)
{
    usleep((useconds_t)(unsigned_long__ms * 1000u));
}

/* ==================== String ==================== */
class String
{
public:
    String() {}
    String(const char *char__ptr_text) : text((char__ptr_text != NULL) ? char__ptr_text : "") {}
    String(const std::string &string__value) : text(string__value) {}
    String(char char__value) : text(1, char__value) {}
    String(int int__value) { func__Stub_FormatNumber((long)int__value); }
    String(unsigned int unsigned_int__value) { func__Stub_FormatNumber((long)unsigned_int__value); }
    String(long long__value) { func__Stub_FormatNumber(long__value); }
    String(unsigned long unsigned_long__value) { func__Stub_FormatNumber((long)unsigned_long__value); }

    String &operator+=(const String &string__other)
    {
        text += string__other.text;
        return *this;
    }
    String &operator+=(const char *char__ptr_other)
    {
        if (char__ptr_other != NULL) { text += char__ptr_other; }
        return *this;
    }
    String &operator+=(char char__other)
    {
        text.push_back(char__other);
        return *this;
    }
    String &operator+=(int int__other)
    {
        text += String(int__other).text;
        return *this;
    }
    String &operator+=(unsigned long unsigned_long__other)
    {
        text += String(unsigned_long__other).text;
        return *this;
    }

    /* مثل String خود آردوینو: کاراکتر در جای مشخص (برای پارسرها) */
    char charAt(unsigned int unsigned_int__index) const
    {
        if (unsigned_int__index >= text.size()) { return '\0'; }
        return text[unsigned_int__index];
    }

    int indexOf(const String &string__needle, unsigned int unsigned_int__from = 0u) const
    {
        return indexOf(string__needle.text.c_str(), unsigned_int__from);
    }
    int indexOf(const char *char__ptr_needle, unsigned int unsigned_int__from = 0u) const
    {
        if ((char__ptr_needle == NULL) || (unsigned_int__from > text.size()))
        {
            return -1;
        }
        size_t size_t__found = text.find(char__ptr_needle, unsigned_int__from);
        return (size_t__found == std::string::npos) ? -1 : (int)size_t__found;
    }
    int indexOf(char char__needle, unsigned int unsigned_int__from = 0u) const
    {
        if (unsigned_int__from > text.size()) { return -1; }
        size_t size_t__found = text.find(char__needle, unsigned_int__from);
        return (size_t__found == std::string::npos) ? -1 : (int)size_t__found;
    }

    String substring(unsigned int unsigned_int__from) const
    {
        if (unsigned_int__from >= text.size()) { return String(); }
        return String(text.substr(unsigned_int__from));
    }
    String substring(unsigned int unsigned_int__from, unsigned int unsigned_int__to) const
    {
        if ((unsigned_int__from >= text.size()) || (unsigned_int__to <= unsigned_int__from))
        {
            return String();
        }
        if (unsigned_int__to > text.size())
        {
            unsigned_int__to = (unsigned int)text.size();
        }
        return String(text.substr(unsigned_int__from, unsigned_int__to - unsigned_int__from));
    }

    int lastIndexOf(char char__needle) const
    {
        size_t size_t__found = text.rfind(char__needle);
        return (size_t__found == std::string::npos) ? -1 : (int)size_t__found;
    }

    void trim(void)
    {
        size_t size_t__begin = text.find_first_not_of(" \t\r\n");
        size_t size_t__end = text.find_last_not_of(" \t\r\n");
        text = (size_t__begin == std::string::npos)
                   ? std::string()
                   : text.substr(size_t__begin, size_t__end - size_t__begin + 1u);
    }
    unsigned int length(void) const { return (unsigned int)text.size(); }
    const char *c_str(void) const { return text.c_str(); }
    long toInt(void) const { return atol(text.c_str()); }
    void reserve(unsigned int unsigned_int__size) { text.reserve(unsigned_int__size); }

    char operator[](unsigned int unsigned_int__index) const
    {
        return (unsigned_int__index < text.size()) ? text[unsigned_int__index] : '\0';
    }

    bool operator==(const String &string__other) const { return text == string__other.text; }
    bool operator==(const char *char__ptr_other) const
    {
        return (char__ptr_other != NULL) && (text == char__ptr_other);
    }
    bool operator!=(const String &string__other) const { return !(*this == string__other); }
    bool operator!=(const char *char__ptr_other) const { return !(*this == char__ptr_other); }

private:
    void func__Stub_FormatNumber(long long__value)
    {
        char char__buffer[24];
        snprintf(char__buffer, sizeof(char__buffer), "%ld", long__value);
        text = char__buffer;
    }

    std::string text;
};

inline String operator+(const String &string__left, const String &string__right)
{
    String string__sum(string__left);
    string__sum += string__right;
    return string__sum;
}

/* ==================== Serial: صف بایت دوطرفه با برد شبیه‌سازی‌شده ==================== */
class SimSerialLink
{
public:
    void setRxBufferSize(size_t size_t__size)
    {
        rxBufferLimit = (size_t__size > 0u) ? size_t__size : rxBufferLimit;
    }
    void begin(unsigned long unsigned_long__baudRate, int int__config)
    {
        (void)int__config;
        baudRateValue = unsigned_long__baudRate;
        rxQueue.clear();
        txQueue.clear();
    }
    int available(void) const { return (int)rxQueue.size(); }
    int read(void)
    {
        if (rxQueue.empty()) { return -1; }
        int int__value = rxQueue.front();
        rxQueue.pop_front();
        return int__value;
    }
    size_t write(const uint8_t *uint8_t__ptr_bytes, size_t size_t__count)
    {
        if (uint8_t__ptr_bytes == NULL) { return 0u; }
        for (size_t size_t__index = 0u; size_t__index < size_t__count; size_t__index++)
        {
            txQueue.push_back(uint8_t__ptr_bytes[size_t__index]);
        }
        return size_t__count;
    }

    /* مسیر برد -> ESP: قاب‌های تله‌متری و دیاگ */
    void boardToEsp(const uint8_t *uint8_t__ptr_bytes, size_t size_t__count)
    {
        for (size_t size_t__index = 0u; size_t__index < size_t__count; size_t__index++)
        {
            rxQueue.push_back(uint8_t__ptr_bytes[size_t__index]);
        }
        while (rxQueue.size() > rxBufferLimit)
        {
            rxQueue.pop_front();
        }
    }
    /* مسیر ESP -> برد: TIME_SYNC و DIAG_REQ */
    bool takeEspByte(uint8_t *uint8_t__ptr_byte)
    {
        if (txQueue.empty()) { return false; }
        *uint8_t__ptr_byte = txQueue.front();
        txQueue.pop_front();
        return true;
    }
    unsigned long baudRate(void) const { return baudRateValue; }

private:
    std::deque<uint8_t> rxQueue;
    std::deque<uint8_t> txQueue;
    size_t rxBufferLimit = 2048u;
    unsigned long baudRateValue = 921600u;
};

extern SimSerialLink Serial;

/* ==================== File و LittleFS: پوشه‌ای روی دیسک ==================== */
class File
{
public:
    File() {}
    File(const std::string &string__path, const char *char__ptr_mode) : path(string__path)
    {
        handle = fopen(path.c_str(), char__ptr_mode);
    }
    File(const File &) = delete;
    File &operator=(const File &) = delete;

    operator bool(void) const { return handle != NULL; }
    bool operator!(void) const { return handle == NULL; }

    size_t print(const char *char__ptr_text)
    {
        if ((handle == NULL) || (char__ptr_text == NULL)) { return 0u; }
        return fwrite(char__ptr_text, 1u, strlen(char__ptr_text), handle);
    }
    size_t print(const String &string__text) { return print(string__text.c_str()); }
    size_t print(int int__value) { return func__Stub_FilePrintNumber("%d", (long)int__value); }
    size_t print(unsigned int unsigned_int__value)
    {
        return func__Stub_FilePrintNumber("%u", (long)unsigned_int__value);
    }
    size_t print(long long__value) { return func__Stub_FilePrintNumber("%ld", long__value); }
    size_t print(unsigned long unsigned_long__value)
    {
        return func__Stub_FilePrintNumber("%lu", (long)unsigned_long__value);
    }

    String readString(void)
    {
        String string__content;
        if (handle == NULL) { return string__content; }

        char char__buffer[256];
        size_t size_t__got = 0u;
        while ((size_t__got = fread(char__buffer, 1u, sizeof(char__buffer) - 1u, handle)) > 0u)
        {
            char__buffer[size_t__got] = '\0';
            string__content += char__buffer;
        }
        return string__content;
    }
    size_t println(const char *char__ptr_text)
    {
        size_t size_t__written = print(char__ptr_text);
        if ((handle != NULL) && (fputc('\n', handle) != EOF)) { size_t__written++; }
        return size_t__written;
    }
    size_t println(const String &string__text) { return println(string__text.c_str()); }
    size_t println(unsigned long unsigned_long__value)
    {
        size_t size_t__written = print(unsigned_long__value);
        if ((handle != NULL) && (fputc('\n', handle) != EOF)) { size_t__written++; }
        return size_t__written;
    }
    int available(void)
    {
        if (handle == NULL) { return 0; }
        long long__here = ftell(handle);
        if (long__here < 0) { return 0; }
        if (fseek(handle, 0, SEEK_END) != 0) { return 0; }
        long long__end = ftell(handle);
        (void)fseek(handle, long__here, SEEK_SET);
        if (long__end < long__here) { return 0; }
        return (int)(long__end - long__here);
    }
    String readStringUntil(char char__terminator)
    {
        String string__line;
        if (handle == NULL) { return string__line; }
        int int__character = 0;
        while ((int__character = fgetc(handle)) != EOF)
        {
            if ((char)int__character == char__terminator) { break; }
            string__line += (char)int__character;
        }
        return string__line;
    }
    void close(void)
    {
        if (handle != NULL)
        {
            fclose(handle);
            handle = NULL;
        }
    }
    ~File() { close(); }

private:
    size_t func__Stub_FilePrintNumber(const char *char__ptr_format, long long__value)
    {
        char char__buffer[24];
        snprintf(char__buffer, sizeof(char__buffer), char__ptr_format, long__value);
        return print(char__buffer);
    }

    std::string path;
    FILE *handle = NULL;
};

/* روی برد خود `LittleFS` یک شیء سراسری است (نه نام‌فضا)، پس برنامه با نقطه
   صدایش می‌زند؛ اینجا هم عیناً همان شکل پیاده می‌شود. ریشهٔ فایل‌سیستم پیش‌فرض
   بیرون مخزن است تا هیچ فایل وضعیتی داخل گیت ساخته نشود. */
class SimLittleFS
{
public:
    bool begin(bool bool__formatOnFail = false);
    File open(const char *char__ptr_path, const char *char__ptr_mode);
    bool remove(const char *char__ptr_path);
    bool exists(const char *char__ptr_path);
};

extern SimLittleFS LittleFS;

/* ==================== WiFi: فقط گزارش، بدون شبکه ==================== */
class SimWiFi
{
public:
    void persistent(bool bool__enabled) { (void)bool__enabled; }
    void mode(int int__value)
    {
        modeValue = int__value;
        if (int__value == WIFI_OFF)
        {
            apActive = false;
        }
    }
    void setSleep(bool bool__enabled) { (void)bool__enabled; }
    void setSleepMode(int int__value) { (void)int__value; }
    void setOutputPower(float float__dbm) { outputPowerDbmValue = float__dbm; }
    bool softAP(const char *char__ptr_ssid, const char *char__ptr_password)
    {
        ssidText = (char__ptr_ssid != NULL) ? char__ptr_ssid : "";
        passwordText = (char__ptr_password != NULL) ? char__ptr_password : "";
        apActive = true;
        printf("[sim] اکسس‌پوینت شبیه‌سازی‌شده: %s / %s (هیچ رادیویی روشن نمی‌شود)\n",
               ssidText.c_str(), passwordText.c_str());
        return true;
    }
    bool softAPdisconnect(bool bool__wifioff = false)
    {
        (void)bool__wifioff;
        apActive = false;
        return true;
    }
    int softAPgetStationNum(void) const { return stationCount; }
    void setStationNum(int int__count) { stationCount = int__count; }
    float outputPowerDbm(void) const { return outputPowerDbmValue; }
    bool isApActive(void) const { return apActive; }
    int currentMode(void) const { return modeValue; }
    const char *ssid(void) const { return ssidText.c_str(); }

private:
    int modeValue = 0;
    int stationCount = 0;
    float outputPowerDbmValue = 20.5f;
    bool apActive = false;
    std::string ssidText;
    std::string passwordText;
};

extern SimWiFi WiFi;

/* ==================== ESP: ریستارت شبیه‌سازی‌شده ==================== */
/* به‌جای ریست واقعی، استثنا پرتاب می‌شود تا حلقهٔ سیمولاتور دوباره setup را
   اجرا کند؛ وضعیت فایل‌سیستم سر جایش می‌ماند. */
struct SimRestartRequest {};

class SimEsp
{
public:
    void restart(void) { throw SimRestartRequest(); }
    /* شناسهٔ چیپ: روی برد واقعی یکتاست و کلید رمزنگاری فایل‌های حساس از آن
       ساخته می‌شود. اینجا ثابت است تا فایل‌های پوشهٔ شبیه‌ساز بین اجراها
       خوانده شوند. */
    uint32_t getChipId(void) const { return 0x00A5C0DEu; }
};

extern SimEsp ESP;

#endif /* SIM_STUB_ARDUINO_H */
