// Credential durability regressions run against the real HTTP and persistence code.
static std::string func__T_ReadCredentialFile(const char *path)
{
    File file = LittleFS.open(path, "r");
    return file ? std::string(file.readString().c_str()) : std::string();
}
static void func__T_TestCredentialDurability(void)
{
    BOOL__G__FsOk = true;
    func__Simple_CryptoKeyInit();
    func__Simple_AuthDefaults();
    func__Simple_AuthSave();
    (void)func__Simple_SetUserPass("admin", "stable-password");
    std::string saved = func__T_ReadCredentialFile(SIMPLE_USERS_FILE);
    SIM_BOOL__G__RefuseWrites = true;
    bool changed = func__Simple_SetUserPass("admin", "lost-password");
    SIM_BOOL__G__RefuseWrites = false;
    func__T_Check(!changed && func__Simple_VerifyUserPass("admin", "stable-password"), "شکست نوشتن رمز مدیر، موفقیت نمی‌دهد و رمز رم را عوض نمی‌کند");
    func__T_Check(func__T_ReadCredentialFile(SIMPLE_USERS_FILE) == saved, "شکست نوشتن رمز فایل معتبر قبلی را خراب نمی‌کند");
    func__Simple_AuthLoad();
    func__T_Check(func__Simple_VerifyUserPass("admin", "stable-password") && !func__Simple_VerifyUserPass("admin", "admin"), "پس از شکست ذخیره و بوت، رمز مدیر قبلی حفظ می‌شود و پیش‌فرض فعال نمی‌شود");
    // Reset the fixture even when the pre-fix implementation corrupted it.
    func__Simple_AuthDefaults();
    (void)func__Simple_SetUserPass("admin", "stable-password");
    func__Simple_AuthLogout();func__Simple_LoginClear();func__T_ClearCookie();
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token", "");
    (void)func__Simple_AuthLogin("admin", "stable-password");func__T_UseTokenCookie();
    SIM_BOOL__G__RefuseWrites = true;
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/setpass", HTTP_POST, "old=stable-password&new=unsaved-password&new2=unsaved-password");
    SIM_BOOL__G__RefuseWrites = false;
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus() >= 500 && func__Simple_VerifyUserPass("admin", "stable-password"), "HTTP تغییر رمز در خطای فلش پاسخ خطا می‌دهد و رمز قبلی معتبر می‌ماند");
    (void)strcpy(CHAR__G__WifiSsid, "stable-network");(void)strcpy(CHAR__G__WifiPass, "stable-wifi-password");func__Simple_WifiSave();
    saved = func__T_ReadCredentialFile(SIMPLE_WIFI_FILE);
    BOOL__G__WifiRebootPending = false;
    SIMPLE_WEB_SERVER_T__G__Server.on("/wifi", HTTP_POST, func__Simple_HttpSetWifi);
    SIM_BOOL__G__RefuseWrites = true;
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/wifi", HTTP_POST, "ssid=lost-network&pass=lost-wifi-password");
    SIM_BOOL__G__RefuseWrites = false;
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus() >= 500 && !BOOL__G__WifiRebootPending && strcmp(CHAR__G__WifiSsid, "stable-network") == 0 && strcmp(CHAR__G__WifiPass, "stable-wifi-password") == 0, "خطای ذخیره وای‌فای نه پیام موفقیت می‌دهد نه ریستارت می‌کند و تنظیمات رم حفظ می‌شود");
    func__T_Check(func__T_ReadCredentialFile(SIMPLE_WIFI_FILE) == saved, "خطای ذخیره وای‌فای فایل قبلی را خراب نمی‌کند");
    func__Simple_WifiLoad();
    func__T_Check(strcmp(CHAR__G__WifiSsid, "stable-network") == 0 && strcmp(CHAR__G__WifiPass, "stable-wifi-password") == 0, "وای‌فای قبلی پس از شکست ذخیره و بوت باقی می‌ماند");
    BOOL__G__FsOk = false;BOOL__G__WifiRebootPending = false;
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/wifi", HTTP_POST, "ssid=unmounted&pass=unmounted-password");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus() >= 500 && !BOOL__G__WifiRebootPending, "حافظه بازنشده اجازه تغییر وای‌فای و ریستارت با تنظیمات موقت نمی‌دهد");
    func__T_Check(!func__Simple_SetUserPass("admin", "unmounted-password"), "تغییر رمز فقط در رم به عنوان ذخیره موفق پذیرفته نمی‌شود");
    BOOL__G__FsOk = true;func__Simple_AuthDefaults();func__Simple_AuthSave();
    func__Simple_AuthLogout();func__T_ClearCookie();SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token", "");
    BOOL__G__WifiRebootPending = false;
}

static void func__T_TestUnsignedCredentialNonce(void)
{
    func__Simple_CryptoKeyInit();
    const uint32_t nonces[] = {0u, 2147483647u, 2147483648u, 4294967295u};
    for (uint32_t nonce : nonces)
    {
        const String plain = "{\"credential\":\"round-trip\"}";
        uint8_t key[32],macKey[32],block[32],mac[32];
        func__Simple_CryptoSubKey("enc", key);func__Simple_CryptoSubKey("mac", macKey);
        String hex;
        for (size_t i=0u;i<plain.length();i++)
        {
            if (i%32u == 0u) func__Simple_CryptoBlock(key, nonce, (uint32_t)(i/32u), block);
            const uint8_t value = (uint8_t)plain[i] ^ block[i%32u];char pair[3];
            func__Simple_CryptoToHex(&value, 1u, pair);hex+=pair;
        }
        func__Simple_CryptoTag(macKey, nonce, hex, mac);char tag[65];func__Simple_CryptoToHex(mac, SIMPLE_CRYPTO_TAG_LEN, tag);
        String encoded = String("{\"e\":1,\"n\":") + String(nonce) + ",\"c\":\"" + hex + "\",\"m\":\"" + tag + "\"}";
        String decoded;
        func__T_Check(func__Simple_CryptoDecrypt(encoded, decoded) == 2u && decoded == plain, "فایل رمز با nonce بدون علامت حتی بالاتر از سقف عدد علامت‌دار درست باز می‌شود");
    }
}

static void func__T_TestCredentialCommitFailures(void)
{
    BOOL__G__FsOk = true;func__Simple_AuthDefaults();
    (void)func__Simple_SetUserPass("admin", "stable-password");
    (void)strcpy(CHAR__G__WifiSsid, "stable-network");(void)strcpy(CHAR__G__WifiPass, "stable-wifi-password");
    func__Simple_WifiSave();func__Simple_AuthLogout();func__Simple_LoginClear();func__T_ClearCookie();
    (void)func__Simple_AuthLogin("admin", "stable-password");func__T_UseTokenCookie();
    const std::string savedUsers = func__T_ReadCredentialFile(SIMPLE_USERS_FILE), savedWifi = func__T_ReadCredentialFile(SIMPLE_WIFI_FILE);
    bool *failures[] = {&SIM_BOOL__G__ShortPrint, &BOOL__T__CredentialOpenFail, &BOOL__T__CredentialReadFail, &BOOL__T__CredentialRenameFail};
    for (bool *failure : failures)
    {
        *failure = true;
        SIMPLE_WEB_SERVER_T__G__Server.simCall("/setpass", HTTP_POST, "old=stable-password&new=unsaved-password&new2=unsaved-password");
        func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus() == 500 && func__Simple_VerifyUserPass("admin", "stable-password") && func__T_ReadCredentialFile(SIMPLE_USERS_FILE) == savedUsers, "نوشتن نیمه، بازکردن، خواندن یا rename ناموفق حساب قبلی را حفظ و HTTP خطا می‌دهد");
        BOOL__G__WifiRebootPending = false;
        SIMPLE_WEB_SERVER_T__G__Server.simCall("/wifi", HTTP_POST, "ssid=unsaved-network&pass=unsaved-wifi-password");
        func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus() == 500 && !BOOL__G__WifiRebootPending && strcmp(CHAR__G__WifiSsid, "stable-network") == 0 && func__T_ReadCredentialFile(SIMPLE_WIFI_FILE) == savedWifi, "خطای هر مرحله ذخیره وای‌فای، فایل قبلی و شبکه فعال را حفظ می‌کند");
        func__T_Check(func__Simple_IsAdmin(), "شکست ذخیره رمز یا وای‌فای نشست مدیر را باطل نمی‌کند");
        *failure = false;
    }
    {
        File interrupted = LittleFS.open(SIMPLE_USERS_FILE ".tmp", "w");interrupted.print("incomplete");interrupted.close();
        func__Simple_AuthLoad();
        func__T_Check(func__Simple_VerifyUserPass("admin", "stable-password"), "بوت پیش از ثبت نهایی، فایل موقت نیمه را نادیده می‌گیرد");
    }
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/setpass", HTTP_POST, "old=stable-password&new=committed-password&new2=committed-password");
    func__Simple_AuthLoad();
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus() == 200 && func__Simple_VerifyUserPass("admin", "committed-password") && !LittleFS.exists(SIMPLE_USERS_FILE ".tmp"), "ذخیره موفق رمز پس از خطا و فایل نیمه، در بوت بعد معتبر است و فایل موقت نمی‌ماند");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/wifi", HTTP_POST, "ssid=committed-network&pass=committed-wifi-password");
    func__Simple_WifiLoad();
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus() == 200 && BOOL__G__WifiRebootPending && strcmp(CHAR__G__WifiSsid, "committed-network") == 0 && strcmp(CHAR__G__WifiPass, "committed-wifi-password") == 0, "وای‌فای فقط پس از ذخیره موفق ریستارت می‌کند و در بوت بعد باقی می‌ماند");
    func__T_Check(func__T_ReadCredentialFile(SIMPLE_USERS_FILE).find("committed-password") == std::string::npos && func__T_ReadCredentialFile(SIMPLE_WIFI_FILE).find("committed-wifi-password") == std::string::npos, "فایل‌های رمز و وای‌فای همچنان رمزنگاری‌شده‌اند");
    FILE *capture = tmpfile();
    int savedStdout = dup(STDOUT_FILENO);
    if (capture != NULL && savedStdout >= 0)
    {
        fflush(stdout);(void)dup2(fileno(capture), STDOUT_FILENO);
        (void)WiFi.softAP("log-safety-test", "do-not-log-this-secret");
        fflush(stdout);(void)dup2(savedStdout, STDOUT_FILENO);(void)close(savedStdout);
        rewind(capture);char output[512] = {0};(void)fread(output, 1u, sizeof(output)-1u, capture);fclose(capture);
        func__T_Check(strstr(output, "do-not-log-this-secret") == NULL && strstr(output, "log-safety-test") != NULL, "شبیه‌ساز نام شبکه را گزارش می‌کند ولی رمز وای‌فای را در خروجی نمی‌نویسد");
    }
    else
    {
        if (capture != NULL) fclose(capture);
        if (savedStdout >= 0) (void)close(savedStdout);
        func__T_Check(false, "امکان گرفتن خروجی برای آزمون محرمانگی رمز موجود است");
    }
    uint32_t nonce = 0u;
    const char *invalid[] = {"", "-1", "+1", "4294967296", "99999999999", "12x", " 1", "1 "};
    for (const char *text : invalid) func__T_Check(!func__Simple_CryptoNonceRead(String(text), &nonce), "nonce خراب، علامت‌دار یا سرریز رد می‌شود");
    func__Simple_AuthDefaults();func__Simple_AuthSave();func__Simple_AuthLogout();func__T_ClearCookie();
    BOOL__G__WifiRebootPending = false;
}
