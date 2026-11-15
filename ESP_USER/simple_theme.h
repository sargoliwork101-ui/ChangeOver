#include <LittleFS.h>
#include "simple_json.h"
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
    if ((int32_t__theme >= 0) && ((uint32_t)int32_t__theme <= (uint32_t)SIMPLE_THEME_MAX))
    {
        UINT8_T__G__Theme = (uint8_t)int32_t__theme;
    }
}
static void func__Simple_WifiSave(void)
{
    String string__plain;
    String string__enc;
    if (!BOOL__G__FsOk) return;
    string__plain = "{\"ssid\":";
    string__plain += func__Simple_JsonQuote(CHAR__G__WifiSsid);
    string__plain += ",\"pass\":";
    string__plain += func__Simple_JsonQuote(CHAR__G__WifiPass);
    string__plain += "}";
    func__Simple_CryptoEncrypt(string__plain, string__enc);
    File file__f = LittleFS.open(SIMPLE_WIFI_FILE, "w");
    if (!file__f) return;
    file__f.print(string__enc);
    file__f.close();
}
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
    if (uint8_t__state == 0u) return;
    String string__ssid, string__pass;
    if (!func__Simple_JsonReadString(string__content, "ssid", string__ssid) ||
        !func__Simple_JsonReadString(string__content, "pass", string__pass) ||
        string__ssid.length() == 0u || string__ssid.length() > SIMPLE_WIFI_SSID_MAX ||
        string__pass.length() < 8u || string__pass.length() > SIMPLE_WIFI_PASS_MAX) return;
    (void)strncpy(CHAR__G__WifiSsid, string__ssid.c_str(), SIMPLE_WIFI_SSID_MAX);
    CHAR__G__WifiSsid[SIMPLE_WIFI_SSID_MAX] = '\0';
    (void)strncpy(CHAR__G__WifiPass, string__pass.c_str(), SIMPLE_WIFI_PASS_MAX);
    CHAR__G__WifiPass[SIMPLE_WIFI_PASS_MAX] = '\0';
    if (uint8_t__state == 1u)
    {
        func__Simple_WifiSave();
    }
}
