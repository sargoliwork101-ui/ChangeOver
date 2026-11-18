/* Battery retention regressions against the real clock, UART parser and log. */
static void func__T_BatteryResetRam(void)
{
    BOOL__G__FsOk = true;
    BOOL__G__TimeValid = false;BOOL__G__TimeFromDisk = false;BOOL__G__TimeBoardSeen = false;
    BOOL__G__TimePushed = false;BOOL__G__BrowserTimePending = false;
    UINT32_T__G__TimeAnchorSec = 0u;UINT32_T__G__TimeAnchorMs = 0u;
    BOOL__G__TimePairValid = false;BOOL__G__TimeBoardLive = false;BOOL__G__TimePairSaveWanted = false;
    BOOL__G__TimePairResetKnown=false;BOOL__G__TimeBoardResetKnown=false;BOOL__G__TimeBoardEpochBlocked=false;
    UINT32_T__G__TimePairResetCount=0u;UINT32_T__G__TimeBoardResetCount=0u;
    BOOL__G__LogClockGap=false;BOOL__G__LogClockGapStored=false;BOOL__G__LogClockGapEmitted=false;BOOL__G__LogClockRetryArmed=false;
    UINT32_T__G__TimePairWallSec = 0u;UINT32_T__G__TimePairUptimeSec = 0u;
    UINT32_T__G__TimeBoardNowSec = 0u;UINT32_T__G__TimeBoardRxMs = 0u;
    UINT32_T__G__LastSavedTimeSec = 0u;UINT32_T__G__TimeFwVersion = 0u;
    INT32_T__G__LogTzSec = 0;INT32_T__G__LastSavedTzSec = 0;
    UINT32_T__G__LogLastBeatSec = 0u;
    BOOL__G__HelloSeen = false;BOOL__G__DiagValid = false;
    UINT32_T__G__BoardUptimeSec = 0u;UINT32_T__G__BoardTimeSec = 0u;
    SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_SOF0;
    UINT32_T__G__LogDayCode = 0xFFFFFFFFu;UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;BOOL__G__LogDayFileReady = false;BOOL__G__LogMarkerDone = false;
    BOOL__G__LogHasPrev = false;BOOL__G__LogRestoredPrev = false;BOOL__G__LogBootRowPending = true;
    BOOL__G__LogLastRowValid = false;UINT32_T__G__LogLastRowMs = 0u;
    BOOL__G__LogLastStepValid = false;UINT32_T__G__LogLastStepMs = 0u;
    BOOL__G__LogLinkDown = false;UINT32_T__G__LogSessionStartMs = 0u;
    UINT16_T__G__LogPendingMask = 0u;UINT8_T__G__LogPendingNote = 0u;
    BOOL__G__LogMainsQueuedValid = false;UINT8_T__G__LogPrevNote = 0u;
    BOOL__G__LogUpdatePending = false;BOOL__A__LogForceRow = false;
    UINT8_T__G__LogRamHead = 0u;UINT8_T__G__LogRamCount = 0u;
    BOOL__G__LogDrainWait = false;BOOL__G__LogScanValid = false;UINT8_T__G__LogIndexCount = 0u;
    BOOL__G__LogCodecCached = false;
    memset(UINT32_T__G__TlmField, 0, sizeof(UINT32_T__G__TlmField));
    BOOL__G__TlmSeen = false;UINT8_T__G__TlmFlags = 0u;UINT8_T__G__TlmFlags2 = 0u;
}

static void func__T_BatteryBoot(void)
{
    func__T_BatteryResetRam();
    func__Simple_TimeLoad();
    func__SimpleLog_Begin();
}

static void func__T_BatteryHello(uint32_t uptime, bool diag = true, uint32_t resets = 1u, uint32_t epoch = 0u)
{
    uint8_t payload[SIMPLE_HELLO_ACK_LEN] = {}, frame[64];
    payload[0] = diag ? 0u : SIMPLE_LINK_PROTOCOL_VERSION;
    unsigned offset = diag ? 9u : 4u;
    for (unsigned i = 0u; i < 4u; i++) payload[offset+i] = (uint8_t)(uptime >> (8u*i));
    if(diag)for(unsigned i=0u;i<4u;i++)
    {
        payload[13u+i]=(uint8_t)(resets>>(8u*i));
        payload[17u+i]=(uint8_t)(epoch>>(8u*i));
    }
    uint16_t len = func__T_MakeFrame(frame, diag ? SIMPLE_MSG_DIAG_DATA : SIMPLE_MSG_HELLO_ACK,
                                    payload, diag ? 23u : SIMPLE_HELLO_ACK_LEN);
    for (uint16_t i = 0u; i < len; i++) func__Simple_ParseByte(frame[i]);
}

static void func__T_BatteryTelemetry(void)
{
    BOOL__G__TlmSeen = true;UINT32_T__G__LastTlmMs = (uint32_t)millis();
    UINT8_T__G__TlmFlags = 0x40u; /* handshake bit is not a logged mode change */
    UINT32_T__G__TlmField[15] = 25000u;
    UINT32_T__G__TlmField[17] = 12500u;UINT32_T__G__TlmField[18] = 12500u;
    func__SimpleLog_OnFrame();
}

static void func__T_TestBatteryClockRetention(void)
{
    (void)printf("\n== ساعت روی باتری و ذخیره امن ==\n");
    LittleFS.format();func__T_BatteryBoot();
    const uint32_t start = 1793835900u;
    func__Simple_TimeSet(start, true);func__Simple_TimeSave();
    func__T_BatteryHello(100u);
    func__T_Check(func__Simple_TimeField(func__T_ReadCredentialFile(SIMPLE_TIME_FILE).c_str(), "\"bu\":") == 100u,
                  "اولین دیاگ برد بلافاصله لنگر ساعت را حتی داخل مهلت پنج دقیقه ذخیره می‌کند");
    func__T_BatteryBoot();
    func__T_Check(!func__Simple_TimeBootReady(), "بوت منتظر بازیابی ساعت از برد می‌ماند");
    func__T_BatteryHello(3700u);
    func__T_Check(func__Simple_NowSec() == start+3600u && func__Simple_TimeBootReady(),
                  "یک ساعت خاموشی ESP با آپتایم و شمارنده ریست فعلی برد و بدون گوشی جبران می‌شود");
    func__T_BatteryBoot();func__T_BatteryHello(7300u, true);
    func__T_Check(func__Simple_NowSec() == start+7200u, "دیاگ هم بدون HELLO زمان خواب را جبران می‌کند");
    func__T_BatteryBoot();
    func__Simple_TimeSet(start+10800u, true);BOOL__G__BrowserTimePending = true;
    func__Simple_TimeSave();func__T_BatteryHello(10900u);
    func__T_Check(func__Simple_NowSec() == start+10800u && BOOL__G__TimePairValid,
                  "ساعت گوشی قبل از HELLO لنگر برد را حذف یا زمان خواب را دوبار حساب نمی‌کند");
    func__T_BatteryBoot();func__T_BatteryHello(14500u);
    func__T_Check(func__Simple_NowSec() == start+14400u,
                  "بعد از همگام‌سازی گوشی، خواب بعدی هم بدون مرورگر درست جبران می‌شود");
    func__T_BatteryBoot();
    UINT32_T__G__TimeAnchorMs -= 310000u;func__Simple_TimeSave();
    func__T_BatteryBoot();func__T_BatteryHello(18100u);
    func__T_Check(func__Simple_NowSec() == start+18000u,
                  "ذخیره در بوت ساکت جفت زمان را نگه می‌دارد و زمان را دوباره نمی‌شمارد");
    uint32_t beforeReset = func__Simple_NowSec();
    func__T_BatteryHello(10u,true,2u);
    func__T_Check(func__Simple_NowSec() == beforeReset && UINT32_T__G__TimePairUptimeSec == 10u,
                  "ریست STM32 ساعت پنل را عقب نمی‌برد و لنگر تازه می‌سازد");
    func__T_BatteryBoot();func__T_BatteryHello(190u,true,2u);
    func__T_Check(func__Simple_NowSec() == beforeReset+180u,
                  "لنگر تازه پس از ریست STM32 فوراً ذخیره شده و خواب بعدی درست است");
    func__Simple_TimeAdoptFromBoard(func__Simple_NowSec()-60u);
    func__T_Check(func__Simple_NowSec() == beforeReset+180u, "دیاگ عقب‌تر حتی در بازه پنج دقیقه ساعت را عقب نمی‌برد");
    UINT32_T__G__TimeAnchorMs -= 30000u;UINT32_T__G__TimeBoardRxMs -= 30000u;
    INT32_T__G__LogTzSec = 12600;func__Simple_TimeSave();
    func__T_BatteryBoot();func__T_BatteryHello(370u,true,2u);
    func__T_Check(func__Simple_NowSec() == beforeReset+360u && INT32_T__G__LogTzSec == 12600,
                  "ذخیره سی ثانیه بعد از پاسخ برد، در خواب بعدی سی ثانیه اضافه نمی‌شمارد");
    std::string saved = func__T_ReadCredentialFile(SIMPLE_TIME_FILE);
    func__Simple_TimeSave();
    func__T_Check(func__T_ReadCredentialFile(SIMPLE_TIME_FILE) == saved,
                  "ذخیره بدون تغییر و پیش از پنج دقیقه فایل ساعت را بازنویسی نمی‌کند");
    bool *faults[] = {&SIM_BOOL__G__RefuseWrites, &SIM_BOOL__G__ShortPrint,
                     &BOOL__T__CredentialOpenFail, &BOOL__T__CredentialReadFail, &BOOL__T__CredentialRenameFail};
    for (bool *fault : faults)
    {
        uint32_t lastSaved = UINT32_T__G__LastSavedTimeSec;
        func__Simple_TimeSet(func__Simple_NowSec()+301u, false);
        *fault = true;func__Simple_TimeSave();
        func__T_Check(func__T_ReadCredentialFile(SIMPLE_TIME_FILE) == saved && UINT32_T__G__LastSavedTimeSec == lastSaved,
                      "خطای نوشتن، نوشتن نیمه، بازکردن، بازخوانی یا ثبت نهایی فایل ساعت قبلی را حفظ می‌کند");
        func__SimpleLog_FwStampWrite(119u);
        func__T_Check(func__T_ReadCredentialFile(SIMPLE_TIME_FILE) == saved,
                      "مسیر ذخیره نسخه نیز هنگام خطای فلش فایل ساعت را خراب نمی‌کند");
        *fault = false;
    }
    func__Simple_TimeSave();saved = func__T_ReadCredentialFile(SIMPLE_TIME_FILE);
    func__T_Check(func__Simple_TimeField(saved.c_str(), "\"fw\":") == 119u && saved.find("\"bu\":0,") == std::string::npos,
                  "تلاش مجدد ساعت، مهر نسخه و لنگر برد را با هم ذخیره می‌کند");
    { File temp = LittleFS.open(SIMPLE_TIME_FILE ".tmp", "w");temp.print("incomplete");temp.close(); }
    func__T_BatteryResetRam();func__Simple_TimeLoad();
    func__T_Check(BOOL__G__TimeValid && BOOL__G__TimePairValid && UINT32_T__G__TimeFwVersion == 119u,
                  "بوت فایل موقت ناقص را نادیده می‌گیرد و ساعت ثبت‌شده را می‌خواند");
    uint32_t wall = func__Simple_NowSec(), pairWall = UINT32_T__G__TimePairWallSec, pairUp = UINT32_T__G__TimePairUptimeSec;
    func__SimpleLog_FwStampWrite(SIMPLE_ESP_FW_VERSION);func__T_BatteryResetRam();func__Simple_TimeLoad();
    func__T_Check(func__Simple_NowSec() == wall && UINT32_T__G__TimePairWallSec == pairWall && UINT32_T__G__TimePairUptimeSec == pairUp,
                  "ثبت نسخه و بوت مجدد جفت زمان و ساعت فعلی را حفظ می‌کند");
}

static void func__T_TestBatteryLogRetention(void)
{
    (void)printf("\n== ۲۷ ساعت باتری، ۵۴۰ بوت و عبور از نیمه‌شب ==\n");
    LittleFS.format();func__T_BatteryBoot();
    const uint32_t start = 1793835900u;
    func__Simple_TimeSet(start, true);func__T_BatteryHello(100u);
    func__T_BatteryTelemetry();func__SimpleLog_Tick();func__Simple_TimeSave();
    bool clockOk = true, quietOk = true;
    for (uint32_t wake = 1u; wake <= 540u; wake++)
    {
        func__T_BatteryBoot();func__T_BatteryHello(100u+wake*180u);
        clockOk = clockOk && func__Simple_NowSec() == start+wake*180u;
        func__T_BatteryTelemetry();func__SimpleLog_Tick();
        quietOk = quietOk && !BOOL__G__LogDayFull && UINT16_T__G__LogPendingMask == 0u;
        if (wake == 6u)
        {
            func__T_Check(BOOL__G__LogHasPrev && UINT16_T__G__LogRowsToday == 0u && !BOOL__G__LogBootRowPending,
                          "فایل خالی روز جدید، حافظه وضعیت روز قبل را از بین نمی‌برد و ردیف بوت اضافی ندارد");
        }
        // Two board replies model a one-minute powered session followed by a two-minute sleep.
        func__T_BatteryHello(100u+wake*180u+58u);func__SimpleLog_Tick();
        clockOk = clockOk && func__Simple_NowSec() == start+wake*180u+58u;
    }
    unsigned rows = 0u;
    func__SimpleLog_ScanDays(true);
    for (uint8_t i = 0u; i < UINT8_T__G__LogIndexCount; i++)
    {
        char path[SIMPLE_LOG_PATH_MAX];func__SimpleLog_PathOf(path, UINT32_T__G__LogIndexDay[i]);
        rows += func__SimpleLog_CountRows(path);
    }
    printf("BATTERY: 540 cold boots, 97258 board seconds, %u rows across %u days\n", rows, UINT8_T__G__LogIndexCount);
    func__T_Check(clockOk, "ساعت در تمام ۵۴۰ بوت، ۲۷ ساعت را بدون گوشی یا ساعت معتبر دیاگ حفظ می‌کند");
    func__T_Check(quietOk && rows == 28u, "۵۴۰ روشن‌شدن فقط یک ردیف آغازین و ۲۷ گزارش ساعتی دارد، نه ۵۴۰ ردیف تکراری");
    func__T_Check(UINT8_T__G__LogIndexCount == 3u, "ردیف‌ها در سه تاریخ درست دو سوی نیمه‌شب ذخیره شده‌اند");
    uint16_t before = UINT16_T__G__LogRowsToday;
    UINT8_T__G__TlmFlags |= 0x02u;func__SimpleLog_OnFrame();
    UINT32_T__G__LogMainsQueuedMs = (uint32_t)millis() - SIMPLE_LOG_MAINS_FLUSH_MS;
    func__SimpleLog_Tick();
    func__T_Check(UINT16_T__G__LogRowsToday == before+1u,
                  "پس از ۲۷ ساعت روی باتری، وصل برق همچنان در مهلت ۲۵۰ میلی‌ثانیه ثبت می‌شود");
    func__T_BatteryBoot();
    BOOL__G__LogUpdatePending = true;
    UINT16_T__G__LogPendingMask = SIMPLE_LOG_CHG_MAINS;
    func__SimpleLog_Tick();
    func__T_Check(BOOL__G__LogBootRowPending && BOOL__G__LogUpdatePending && UINT16_T__G__LogPendingMask == SIMPLE_LOG_CHG_MAINS,
                  "انتظار ساعت، پرچم بوت یا بروزرسانی یا رویداد معلق را مصرف نمی‌کند");
    func__T_BatteryHello(100u+540u*180u+180u);func__T_BatteryTelemetry();
    before = func__SimpleLog_CountRows("/evt-20261106.csv");
    func__SimpleLog_Tick();func__SimpleLog_Tick();
    func__T_Check(!BOOL__G__LogUpdatePending && !BOOL__G__LogBootRowPending &&
                  func__SimpleLog_CountRows("/evt-20261106.csv") == before+1u,
                  "پس از رسیدن ساعت، بروزرسانی و رویداد معلق دقیقاً یک بار ثبت می‌شوند");
    // Quiet sleep must not erase the last real state, including excluded flag bits.
    func__T_BatteryBoot();func__T_BatteryHello(100u+540u*180u+360u);
    func__T_BatteryTelemetry();func__SimpleLog_Tick();
    func__T_Check(UINT16_T__G__LogPendingMask == 0u, "بیت دست‌دادن برد پس از بازیابی ردیف، تغییر مود جعلی نمی‌سازد");
    before = UINT16_T__G__LogRowsToday;
    BOOL__G__LogLastRowValid = true;UINT32_T__G__LogLastRowMs = (uint32_t)millis();
    UINT32_T__G__LogLastBeatSec = func__Simple_NowSec() - SIMPLE_LOG_HEARTBEAT_MS/1000u;
    func__SimpleLog_Tick();
    func__T_Check(UINT16_T__G__LogPendingMask == 0u && UINT8_T__G__LogPendingNote == SIMPLE_LOG_NOTE_HEARTBEAT,
                  "گزارش ساعتی که در پنجره ادغام منتظر مانده یادداشت معلق خود را نگه می‌دارد");
    UINT32_T__G__LogLastRowMs -= SIMPLE_LOG_MIN_INTERVAL_MS;
    UINT32_T__G__LogLastBeatSec = func__Simple_NowSec(); // no new heartbeat to mask a broken flush
    func__SimpleLog_Tick();
    func__T_Check(UINT16_T__G__LogRowsToday == before+1u && UINT8_T__G__LogPendingNote == 0u,
                  "یادداشت تنها، بدون تغییر جدید، بعد از پنجره ادغام روی فلش ثبت می‌شود");
    func__T_BatteryResetRam();LittleFS.format();
}
