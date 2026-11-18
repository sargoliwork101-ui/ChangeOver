// Retained administrator credentials and mutable Wi-Fi persistence regressions.
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
    (void)func__T_SeedAdminPassword("stable-password");
    std::string saved = func__T_ReadCredentialFile(SIMPLE_USERS_FILE);
    SIM_BOOL__G__RefuseWrites = true;
    bool stored = func__Simple_AuthSave();
    SIM_BOOL__G__RefuseWrites = false;
    func__T_Check(!stored && func__T_ReadCredentialFile(SIMPLE_USERS_FILE)==saved, "شکست ذخیره حساب، فایل معتبر قبلی را خراب نمی‌کند");
    func__Simple_AuthLoad();
    func__T_Check(func__Simple_VerifyUserPass("admin", "stable-password") && !func__Simple_VerifyUserPass("admin", "admin"), "رمز ذخیره‌شده سفارشی بعد از بازخوانی حفظ می‌شود");
    func__Simple_AuthLogout();func__Simple_LoginClear();func__T_ClearCookie();
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token", "");
    (void)func__Simple_AuthLogin("admin", "stable-password");func__T_UseTokenCookie();
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
    (void)func__T_SeedAdminPassword("stable-password");
    (void)strcpy(CHAR__G__WifiSsid, "stable-network");(void)strcpy(CHAR__G__WifiPass, "stable-wifi-password");
    func__Simple_WifiSave();func__Simple_AuthLogout();func__Simple_LoginClear();func__T_ClearCookie();
    (void)func__Simple_AuthLogin("admin", "stable-password");func__T_UseTokenCookie();
    const std::string savedUsers = func__T_ReadCredentialFile(SIMPLE_USERS_FILE), savedWifi = func__T_ReadCredentialFile(SIMPLE_WIFI_FILE);
    bool *failures[] = {&SIM_BOOL__G__ShortPrint, &BOOL__T__CredentialOpenFail, &BOOL__T__CredentialReadFail, &BOOL__T__CredentialRenameFail};
    for (bool *failure : failures)
    {
        *failure = true;
        bool stored = func__Simple_AuthSave();
        func__T_Check(!stored && func__T_ReadCredentialFile(SIMPLE_USERS_FILE)==savedUsers, "نوشتن نیمه، بازکردن، بازخوانی یا rename ناموفق فایل حساب قبلی را حفظ می‌کند");
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
    bool stored = func__Simple_AuthSave();func__Simple_AuthLoad();
    func__T_Check(stored && func__Simple_VerifyUserPass("admin", "stable-password") && !LittleFS.exists(SIMPLE_USERS_FILE ".tmp"), "ذخیره دوباره همان حساب پس از خطا، رمز موجود را در بوت حفظ می‌کند");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/wifi", HTTP_POST, "ssid=committed-network&pass=committed-wifi-password");
    func__Simple_WifiLoad();
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus() == 200 && BOOL__G__WifiRebootPending && strcmp(CHAR__G__WifiSsid, "committed-network") == 0 && strcmp(CHAR__G__WifiPass, "committed-wifi-password") == 0, "وای‌فای فقط پس از ذخیره موفق ریستارت می‌کند و در بوت بعد باقی می‌ماند");
    func__T_Check(func__T_ReadCredentialFile(SIMPLE_USERS_FILE).find("stable-password") == std::string::npos && func__T_ReadCredentialFile(SIMPLE_WIFI_FILE).find("committed-wifi-password") == std::string::npos, "فایل‌های رمز و وای‌فای همچنان رمزنگاری‌شده‌اند");
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

static void func__T_TestRetainedAdminPassword(void)
{
    BOOL__G__FsOk=true;LittleFS.format();func__Simple_CryptoKeyInit();
    (void)func__T_SeedAdminPassword("retained &+ password");
    const std::string committed=func__T_ReadCredentialFile(SIMPLE_USERS_FILE);
    {File legacy=LittleFS.open("/boot_no_login.json","w");legacy.print("{\"noLogin\":1}");legacy.close();}
    bool kept=true;
    for(unsigned wake=0u;wake<540u;wake++)
    {
        func__Simple_AuthLogout();func__Simple_LoginClear();func__T_ClearCookie();
        func__Simple_AuthLoad();
        kept=kept && func__Simple_VerifyUserPass("admin","retained &+ password") &&
            !func__Simple_VerifyUserPass("admin","admin") && func__T_ReadCredentialFile(SIMPLE_USERS_FILE)==committed;
    }
    func__T_Check(kept,"۵۴۰ بار بارگذاری در بیداری باتری و پرچم قدیمی، رمز سفارشی را به پیش‌فرض برنمی‌گرداند و فایل را بازنویسی نمی‌کند");
    const std::string sketch=func__T_ReadEspUserSource("ESP_USER.ino"), auth=func__T_ReadEspUserSource("simple_auth.h");
    func__T_Check(!sketch.empty() && sketch.find("Recovery")==std::string::npos && sketch.find("/setpass")==std::string::npos &&
                  auth.find("func__Simple_SetUserPass")==std::string::npos && auth.find("func__Simple_Recovery")==std::string::npos,
                  "مسیر واقعی راه‌اندازی و حلقه، بازیابی زمان‌دار و تغییر رمز ندارد");
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token", "");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/setpass",HTTP_POST,"old=retained+%26%2B+password&new=changed-password");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==404,"تغییر رمز مستقیم برای مهمان هم مسیر ندارد");
    func__T_Check(func__Simple_AuthLogin("admin","retained &+ password")==1,"ورود مدیر با رمز سفارشی ذخیره‌شده همچنان کار می‌کند");
    func__T_UseTokenCookie();
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/setpass",HTTP_POST,"old=retained+%26%2B+password&new=changed-password");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==404 && func__T_ReadCredentialFile(SIMPLE_USERS_FILE)==committed && func__Simple_IsAdmin(),
                  "حتی درخواست مدیر هم رمز را تغییر نمی‌دهد و نشست موجود حفظ می‌شود");
    SIMPLE_WEB_SERVER_T__G__Server.simClear();func__Simple_HttpEventFormat();func__Simple_AuthLoad();
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==200 && func__Simple_VerifyUserPass("admin","retained &+ password") && func__Simple_IsAdmin(),
                  "فرمت کامل هم رمز نگه‌داشته‌شده و نشست مدیر را حفظ می‌کند");
    func__Simple_AuthLogout();func__T_ClearCookie();func__Simple_AuthDefaults();func__Simple_AuthSave();
}
