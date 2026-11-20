/* Verify panel-only factory reset without invoking any board command. */
static void func__T_TestFactoryReset(void)
{
    LittleFS.format();BOOL__G__FsOk=true;BOOL__G__FactoryResetPending=false;
    File sentinel=LittleFS.open("/factory-sentinel","w");sentinel.print("keep");sentinel.close();
    func__Simple_AuthDefaults();func__Simple_AuthLogout();func__T_ClearCookie();
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token","");
    SIMPLE_WEB_SERVER_T__G__Server.simClear();func__Simple_HttpFactoryReset();
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==403 && LittleFS.exists("/factory-sentinel"),"factory reset rejects unsigned callers");
    func__Simple_LoginClear();func__T_Login("admin","admin");
    SIMPLE_WEB_SERVER_T__G__Server.simClear();func__Simple_HttpFactoryReset();
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==400 && LittleFS.exists("/factory-sentinel"),"factory reset requires explicit erase confirmation");
    SIMPLE_WEB_SERVER_T__G__Server.on("/test-factory",HTTP_POST,func__Simple_HttpFactoryReset);
    BOOL__T__FormatFail=true;
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/test-factory",HTTP_POST,"confirm=ERASE_ESP");
    BOOL__T__FormatFail=false;
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==500 && !BOOL__G__FactoryResetPending && LittleFS.exists("/factory-sentinel"),"failed format is never reported as a successful factory reset");
    for(const char *name:{SIMPLE_USERS_FILE,SIMPLE_WIFI_FILE,SIMPLE_TIME_FILE,SIMPLE_THEME_FILE,SIMPLE_DIAG_FILE,SIMPLE_CHARGE_HISTORY_FILE,SIMPLE_CHARTS_FILE,"/params.json","/evt-20261104.csv","/other-private-file"})
    {File f=LittleFS.open(name,"w");f.print("old-private-value");f.close();}
    uint8_t byte;while(Serial.takeEspByte(&byte)){}
    UINT32_T__G__ChargeCountCh1=123u;UINT32_T__G__ChargeCountCh2=456u;
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/test-factory",HTTP_POST,"confirm=ERASE_ESP");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==200 && SIMPLE_WEB_SERVER_T__G__Server.simBody().find("\"erased\":1")!=std::string::npos && BOOL__G__FactoryResetPending,"confirmed factory erase schedules ESP restart");
    bool empty=true;
    for(const char *name:{SIMPLE_USERS_FILE,SIMPLE_WIFI_FILE,SIMPLE_TIME_FILE,SIMPLE_THEME_FILE,SIMPLE_DIAG_FILE,SIMPLE_CHARGE_HISTORY_FILE,SIMPLE_CHARTS_FILE,"/params.json","/evt-20261104.csv","/other-private-file","/factory-sentinel","/fs-check"})empty=empty && !LittleFS.exists(name);
    func__T_Check(empty && !BOOL__G__FsOk && !func__Simple_IsAdmin(),"factory erase leaves no old data or credentials and revokes the session");
    func__T_Check(!Serial.takeEspByte(&byte) && UINT32_T__G__ChargeCountCh1==123u && UINT32_T__G__ChargeCountCh2==456u,"factory erase sends no UART commands and does not reset board counters");
    BOOL__G__FactoryResetPending=false;BOOL__G__FsOk=true;
    func__Simple_AuthLoad();func__Simple_WifiLoad();func__Simple_ThemeLoad();
    func__T_Check(func__Simple_VerifyUserPass("admin","admin") && strcmp(CHAR__G__WifiSsid,SIMPLE_WIFI_AP_SSID)==0 && strcmp(CHAR__G__WifiPass,SIMPLE_WIFI_AP_PASS)==0 && UINT8_T__G__Theme==0u,"empty panel restores default admin, Wi-Fi and theme");
    func__T_Login("admin","admin");BOOL__T__MountFail=true;
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/test-factory",HTTP_POST,"confirm=ERASE_ESP");BOOL__T__MountFail=false;
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==500 && BOOL__G__FactoryResetPending && SIMPLE_WEB_SERVER_T__G__Server.simBody().find("\"erased\":1")!=std::string::npos,"erase followed by mount failure reports uncertainty and still blocks old RAM saves");
    BOOL__G__FactoryResetPending=false;BOOL__G__FsOk=true;
}
