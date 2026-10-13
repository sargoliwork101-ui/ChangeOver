/* simple_theme.h - مدیریت تم‌ها (تیره/روشن) و وای‌فای رمزنگاری‌شده
   فقط فارسی - ذخیره در LittleFS */

#include <LittleFS.h>

/* ==================== بارگذاری تم ==================== */
static void func__Simple_ThemeLoad(void)
{
    UINT8_T__G__Theme = 0u;
    if (!BOOL__G__FsOk) return;
    if (!LittleFS.exists(SIMPLE_THEME_FILE)) return;
    File file__f = LittleFS.open(SIMPLE_THEME_FILE, "r");
    if (!file__f) return;
    String string__content = file__f.readString();
    file__f.close();
    int32_t int32_t__pos = string__content.indexOf("\"theme\":");
    if (int32_t__pos < 0) return;
    int32_t__pos += 8;
    int32_t int32_t__end = string__content.indexOf("}", int32_t__pos);
    if (int32_t__end < 0) int32_t__end = string__content.length();
    String string__val = string__content.substring(int32_t__pos, int32_t__end);
    int32_t int32_t__theme = string__val.toInt();
    if (int32_t__theme >= 0 && int32_t__theme <= SIMPLE_THEME_MAX)
    {
        UINT8_T__G__Theme = (uint8_t)int32_t__theme;
    }
}

/* ==================== ذخیره تم ==================== */
static void func__Simple_ThemeSave(void)
{
    if (!BOOL__G__FsOk) return;
    File file__f = LittleFS.open(SIMPLE_THEME_FILE, "w");
    if (!file__f) return;
    file__f.print("{\"theme\":");
    file__f.print(UINT8_T__G__Theme);
    file__f.print("}");
    file__f.close();
}

/* ==================== تنظیم تم ==================== */
static bool func__Simple_SetTheme(uint8_t uint8_t__theme)
{
    if (uint8_t__theme > SIMPLE_THEME_MAX) return false;
    UINT8_T__G__Theme = uint8_t__theme;
    func__Simple_ThemeSave();
    return true;
}

/* ==================== وای‌فای - ذخیرهٔ رمزنگاری‌شده ====================
   نام و رمز وای‌فای روی فلش رمز می‌شود؛ قالب فایل همان قالب فایل‌های
   حساس است ({"e":1,"n":..,"c":..,"m":..}) تا رمز خوانا نماند. */
static void func__Simple_WifiSave(void)
{
    String string__plain;
    String string__enc;

    if (!BOOL__G__FsOk) return;

    string__plain = "{\"ssid\":\"";
    string__plain += CHAR__G__WifiSsid;
    string__plain += "\",\"pass\":\"";
    string__plain += CHAR__G__WifiPass;
    string__plain += "\"}";

    func__Simple_CryptoEncrypt(string__plain, string__enc);

    File file__f = LittleFS.open(SIMPLE_WIFI_FILE, "w");
    if (!file__f) return;
    file__f.print(string__enc);
    file__f.close();
}

/* ==================== وای‌فای - بارگذاری ====================
   تا وقتی کلید نخورده یا فایل دست‌کاری شده، پیش‌فرض‌های همین فایل می‌مانند. */
static void func__Simple_WifiLoad(void)
{
    uint8_t uint8_t__state;
    String string__content;

    (void)strncpy(CHAR__G__WifiSsid, SIMPLE_WIFI_AP_SSID, SIMPLE_WIFI_SSID_MAX);
    CHAR__G__WifiSsid[SIMPLE_WIFI_SSID_MAX] = '\0';
    (void)strncpy(CHAR__G__WifiPass, SIMPLE_WIFI_AP_PASS, SIMPLE_WIFI_PASS_MAX);
    CHAR__G__WifiPass[SIMPLE_WIFI_PASS_MAX] = '\0';
    if (!BOOL__G__FsOk) return;
    if (!LittleFS.exists(SIMPLE_WIFI_FILE)) return;
    File file__f = LittleFS.open(SIMPLE_WIFI_FILE, "r");
    if (!file__f) return;
    String string__raw = file__f.readString();
    file__f.close();

    uint8_t__state = func__Simple_CryptoDecrypt(string__raw, string__content);
    if (uint8_t__state == 0u) return;   /* رمزشده ولی برچسب نخورد - دست نزن */

    int32_t int32_t__ssidPos = string__content.indexOf("\"ssid\":\"");
    if (int32_t__ssidPos >= 0)
    {
        int32_t__ssidPos += 8;
        int32_t int32_t__end = string__content.indexOf("\"", int32_t__ssidPos);
        if (int32_t__end > int32_t__ssidPos)
        {
            String string__ssid = string__content.substring(int32_t__ssidPos, int32_t__end);
            if (string__ssid.length() > 0u && string__ssid.length() <= SIMPLE_WIFI_SSID_MAX)
            {
                (void)strncpy(CHAR__G__WifiSsid, string__ssid.c_str(), SIMPLE_WIFI_SSID_MAX);
                CHAR__G__WifiSsid[SIMPLE_WIFI_SSID_MAX] = '\0';
            }
        }
    }
    int32_t int32_t__passPos = string__content.indexOf("\"pass\":\"");
    if (int32_t__passPos >= 0)
    {
        int32_t__passPos += 8;
        int32_t int32_t__end = string__content.indexOf("\"", int32_t__passPos);
        if (int32_t__end > int32_t__passPos)
        {
            String string__pass = string__content.substring(int32_t__passPos, int32_t__end);
            if (string__pass.length() >= 8u && string__pass.length() <= SIMPLE_WIFI_PASS_MAX)
            {
                (void)strncpy(CHAR__G__WifiPass, string__pass.c_str(), SIMPLE_WIFI_PASS_MAX);
                CHAR__G__WifiPass[SIMPLE_WIFI_PASS_MAX] = '\0';
            }
        }
    }

    if (uint8_t__state == 1u)
    {
        /* قالب خوانای قدیمی بود؛ همین حالا رمزشده ذخیره کن */
        func__Simple_WifiSave();
    }
}
