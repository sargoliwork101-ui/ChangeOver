/* Delayed clock recovery and durable missing-time warning regressions. */
static unsigned func__T_ClockWarningRows(uint32_t *lastEpoch = nullptr)
{
    func__SimpleLog_ScanDays(true);unsigned count=0u;
    for(uint8_t day=0u;day<UINT8_T__G__LogIndexCount;day++)
    {
        char path[SIMPLE_LOG_PATH_MAX];func__SimpleLog_PathOf(path,UINT32_T__G__LogIndexDay[day]);
        std::istringstream rows(func__T_DayText(path));std::string row;
        while(std::getline(rows,row))if(func__SimpleLog_RowField(row.c_str(),3u)==SIMPLE_LOG_NOTE_CLOCK_GAP)
        {
            count++;if(lastEpoch)*lastEpoch=func__SimpleLog_RowEpoch(row.c_str());
        }
    }
    return count;
}
static void func__T_ClockGapWait(uint32_t bootAge)
{
    UINT32_T__G__TimeBootMs=(uint32_t)millis()-bootAge;
    BOOL__G__LogClockRetryArmed=false;func__SimpleLog_Tick();
}
static void func__T_TestDelayedClockGap(void)
{
    printf("\n== دریافت دیرهنگام ساعت و هشدار پایدار ۴۰ ثانیه ==\n");
    LittleFS.format();func__T_BatteryBoot();func__Simple_ChartsClear();
    const uint32_t base=1793836790u;
    func__Simple_TimeSet(base,true);func__T_BatteryHello(100u,true,7u);func__Simple_TimeSave();
    const std::string committed=func__T_ReadCredentialFile(SIMPLE_TIME_FILE);
    func__T_BatteryBoot();func__T_BatteryTelemetry();
    func__T_BatteryHello(235u,false); // HELLO has uptime but no reset identity.
    func__T_ClockGapWait(8100u);
    func__Simple_ChartAddTimed(50u,24000u,25000u);
    func__T_Check(!func__Simple_TimeBootReady() && UINT16_T__G__Chart1D_Count==0u && UINT16_T__G__LogRowsToday==0u,
                  "با وجود تله‌متری و HELLO، هشت ثانیه بدون تأیید ریست نه نمودار می‌سازد نه لاگ تاریخ‌دار");
    UINT32_T__G__TimeAnchorMs-=310000u;func__Simple_TimeSave();
    func__T_Check(func__T_ReadCredentialFile(SIMPLE_TIME_FILE)==committed,"انتظار طولانی ساعت حدسی را روی فایل معتبر ذخیره نمی‌کند");
    const uint8_t oldErrors=UINT8_T__G__DiagErrorCount,oldPower=UINT8_T__G__PowerEventCount;
    UINT32_T__G__LastLocalFaultMask=0u;func__Simple_LocalRecordFault(1u,0u,25000u,12500u,12500u);
    BOOL__G__LastLocalInputValid=true;BOOL__G__LastLocalInputPresent=true;
    func__Simple_LocalRecordPower(false);func__Simple_LocalRecordPower(true);
    func__T_Check(UINT8_T__G__DiagErrorCount==oldErrors && UINT8_T__G__PowerEventCount==oldPower,
                  "مسیرهای همسایه خطا و قطعی هم با ساعت کهنه تاریخچه نمی‌سازند؛ مقادیر زنده باقی‌اند");
    func__T_ClockGapWait(39000u);
    func__T_Check(!LittleFS.exists(SIMPLE_TIME_GAP_FILE) && !BOOL__G__LogClockGap,"قبل از چهل ثانیه هشدار زودرس نداریم");
    func__T_ClockGapWait(40000u);
    func__T_Check(BOOL__G__LogClockGapStored && LittleFS.exists(SIMPLE_TIME_GAP_FILE) && !func__Simple_TimeBootReady(),
                  "در مرز چهل ثانیه هشدار بدون تاریخ روی فلش می‌ماند ولی ساعت معتبر نمی‌شود");
    std::string marker=func__T_ReadCredentialFile(SIMPLE_TIME_GAP_FILE);
    SIM_BOOL__G__RefuseWrites=true;func__T_ClockGapWait(55000u);SIM_BOOL__G__RefuseWrites=false;
    func__T_Check(func__T_ReadCredentialFile(SIMPLE_TIME_GAP_FILE)==marker && UINT16_T__G__LogRowsToday==0u,
                  "ادامه انتظار هشدار را تکراری بازنویسی نمی‌کند و روز اشتباه نمی‌سازد");
    SIMPLE_WEB_SERVER_T__G__Server.simClear();func__Simple_HttpEventLog();
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simBody().find(SIMPLE_LOG_CLOCK_GAP_ROW)!=std::string::npos,
                  "هشدار انتظار همان موقع در لاگ عمومی با زمان صفر دیده می‌شود");
    SIMPLE_WEB_SERVER_T__G__Server.simClear();func__Simple_HttpDownloadAllDays();
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simBody().find(SIMPLE_LOG_CLOCK_GAP_ROW)!=std::string::npos,
                  "خروجی یکجای CSV هم هشدار بدون تاریخ را نگه می‌دارد");
    SIMPLE_WEB_SERVER_T__G__Server.simClear();func__Simple_HttpTelemetry();
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simBody().find("\"timeOk\":0")!=std::string::npos &&
                  SIMPLE_WEB_SERVER_T__G__Server.simBody().find("\"bt\":0")!=std::string::npos,"پاسخ عمومی ساعت قدیمی را زمان معتبر معرفی نمی‌کند");
    func__T_BatteryBoot();
    func__T_Check(BOOL__G__LogClockGap && !func__Simple_TimeBootReady(),"هشدار در بوت بعدی، پیش از چهل ثانیه جدید هم حفظ می‌شود");
    func__T_BatteryHello(280u,true,7u);
    func__T_Check(func__Simple_TimeBootReady() && func__Simple_NowSec()==base+180u,
                  "پاسخ دیرهنگام بردِ ریست‌نشده، همان لحظه سه دقیقه را جبران می‌کند؛ حتی پس از نیمه‌شب و بدون گوشی");
    func__T_BatteryTelemetry();func__SimpleLog_Tick();uint32_t warningEpoch=0u;
    func__T_Check(!LittleFS.exists(SIMPLE_TIME_GAP_FILE) && func__T_ClockWarningRows(&warningEpoch)==1u && warningEpoch==base+180u,
                  "هشدار فقط بعد از ثبت روی فلش به روز صحیحِ بازیابی منتقل و نشانگر پاک می‌شود");
    func__Simple_ChartAddTimed(50u,24000u,25000u);
    func__T_Check(UINT16_T__G__Chart1D_Count==1u && UINT32_T__G__Chart1D_Time[0]==base+180u,
                  "اولین نمونه تازه با ساعت صحیح ثبت می‌شود و فاصله بدون ساعت ساختگی پر نمی‌شود");
    for(unsigned i=0u;i<10u;i++)func__SimpleLog_Tick();
    func__T_Check(func__T_ClockWarningRows()==1u,"حلقه‌های بعد هشدار بازیابی را تکرار نمی‌کنند");
    // A rebooted STM32 can already have a larger uptime than the persisted pair.
    func__T_BatteryBoot();func__T_BatteryHello(5000u,true,8u,base+4900u);
    func__T_Check(!func__Simple_TimeBootReady() && BOOL__G__TimeBoardEpochBlocked,
                  "تغییر شمارنده ریست حتی با آپتایم بزرگ‌تر و تاریخ ظاهراً معتبر، زمان حدسی را تأیید نمی‌کند");
    func__T_BatteryHello(5010u,true,8u,base+4910u);
    func__T_Check(!func__Simple_TimeBootReady(),"پاسخ‌های تکراری برد ریست‌شده هم ساعت کهنه را معتبر نمی‌کنند");
    func__Simple_TimeSet(base+9000u,true);BOOL__G__BrowserTimePending=true;func__Simple_TimeSave();
    func__T_Check(func__Simple_TimeBootReady() && !BOOL__G__TimeBoardEpochBlocked,
                  "ساعت تازه گوشی بن‌بست ریست هم‌زمان را بدون تغییر STM32 رفع می‌کند");
    // No initial time, absent files, and marker write failures use the same gate.
    LittleFS.format();func__T_BatteryBoot();
    func__T_Check(!func__Simple_TimeBootReady(),"حافظه خالی هم مجوز ثبت با تاریخ ساختگی نمی‌دهد");
    bool *faults[]={&SIM_BOOL__G__RefuseWrites,&SIM_BOOL__G__ShortPrint,
                    &BOOL__T__CredentialOpenFail,&BOOL__T__CredentialReadFail,&BOOL__T__CredentialRenameFail};
    for(bool *fault:faults)
    {
        *fault=true;func__T_ClockGapWait(40000u);*fault=false;
        func__T_Check(BOOL__G__LogClockGap && !LittleFS.exists(SIMPLE_TIME_GAP_FILE) && !BOOL__G__LogClockGapStored,
                      "نوشتن ناقص یا فلش غیرقابل‌نوشتن، ذخیره موفق هشدار گزارش نمی‌شود");
    }
    func__T_ClockGapWait(45000u);
    func__T_Check(LittleFS.exists(SIMPLE_TIME_GAP_FILE),"پس از رفع خطای فلش، هشدار دوباره ذخیره می‌شود");
    func__T_BatteryHello(50u,true,3u,base+10000u);
    SIM_BOOL__G__RefuseWrites=true;BOOL__G__LogClockRetryArmed=false;func__SimpleLog_ClockGapTick();SIM_BOOL__G__RefuseWrites=false;
    func__T_Check(LittleFS.exists(SIMPLE_TIME_GAP_FILE) && !BOOL__G__LogClockGapEmitted,
                  "شکست ثبت لاگ پس از بازیابی، نشانگر پایدار را زودتر پاک نمی‌کند");
    BOOL__G__LogClockRetryArmed=false;func__SimpleLog_ClockGapTick();
    func__T_Check(!LittleFS.exists(SIMPLE_TIME_GAP_FILE) && func__T_ClockWarningRows()==1u,
                  "تلاش مجدد پس از خطای ثبت، هشدار را نگه می‌دارد و بعد پاک می‌کند");
    LittleFS.format();func__T_BatteryBoot();func__T_ClockGapWait(40000u);
    func__Simple_TimeSet(base+11000u,true);BOOL__T__RemoveFail=true;
    BOOL__G__LogClockRetryArmed=false;func__SimpleLog_ClockGapTick();
    func__T_Check(BOOL__G__LogClockGapEmitted && LittleFS.exists(SIMPLE_TIME_GAP_FILE) && func__T_ClockWarningRows()==1u,
                  "خطای حذف نشانگر پس از ثبت هشدار، خودِ نشانگر را برای تلاش مجدد نگه می‌دارد");
    BOOL__G__LogClockRetryArmed=false;func__SimpleLog_ClockGapTick();
    func__T_Check(func__T_ClockWarningRows()==1u,"تلاش مجدد حذف، هشدار تاریخ‌دار را در همان اجرا تکرار نمی‌کند");
    BOOL__T__RemoveFail=false;BOOL__G__LogClockRetryArmed=false;func__SimpleLog_ClockGapTick();
    func__T_Check(!LittleFS.exists(SIMPLE_TIME_GAP_FILE) && !BOOL__G__LogClockGap,"پس از رفع خطای حذف، رخداد پایان می‌یابد");
    LittleFS.format();func__T_BatteryBoot();func__T_ClockGapWait(40000u);
    func__SimpleLog_ResetStateAfterFormat();LittleFS.format();
    func__T_Check(!BOOL__G__LogClockGap && !LittleFS.exists(SIMPLE_TIME_GAP_FILE),"فرمت صریح نشانگر هشدار و وضعیت رم آن را پاک می‌کند");
    func__T_BatteryBoot();func__Simple_TimeSet(base,true);func__T_BatteryHello(100u,true,9u);func__Simple_TimeSave();
    func__T_BatteryBoot();func__T_ClockGapWait(9000u);func__T_BatteryHello(130u,true,9u);func__SimpleLog_Tick();
    func__T_Check(func__Simple_TimeBootReady() && !BOOL__G__LogClockGap && func__T_ClockWarningRows()==0u,
                  "پاسخ دیرتر از هشت ولی زودتر از چهل ثانیه ساعت را اصلاح می‌کند، بدون هشدار بی‌مورد");
    func__Simple_TimeSet(base,true);
}
