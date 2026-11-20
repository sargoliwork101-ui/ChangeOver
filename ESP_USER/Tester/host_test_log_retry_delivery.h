/* @file host_test_log_retry_delivery.h
 * @brief Regress calendar retry, exact append sizing and durable clock-warning delivery.
 */
static void func__T_TestDayRetryInitialization(void)
{
    const uint32_t base=1793836800u;
    LittleFS.format();func__T_BatteryResetRam();func__Simple_TimeSet(base,true);
    char old[SIMPLE_LOG_PATH_MAX];func__SimpleLog_PathOf(old,func__SimpleLog_DateCode(base/86400u-60u));
    func__T_ChartWrite(old,"old fixture");
    SIM_BOOL__G__RefuseWrites=true;bool first=func__SimpleLog_ResolveDay(true);SIM_BOOL__G__RefuseWrites=false;
    bool retried=func__SimpleLog_ResolveDay(true);
    func__T_Check(!first&&retried&&!LittleFS.exists(old),"successful retry after initial day-write failure also expires the 60-day-old file");
    char path[SIMPLE_LOG_PATH_MAX];uint32_t day;func__T_CapFixture(200u,path,day);
    func__T_BatteryResetRam();func__Simple_TimeSet(base,true);BOOL__G__FsOk=false;
    first=func__SimpleLog_ResolveDay(true);BOOL__G__FsOk=true;retried=func__SimpleLog_ResolveDay(true);
    func__T_Check(!first&&retried&&UINT16_T__G__LogRowsToday==200u&&BOOL__G__LogDayFull,
        "retry after unavailable storage recounts an existing full day before applying its cap");
}
static void func__T_TestExactAppendSizing(void)
{
    char path[SIMPLE_LOG_PATH_MAX];uint32_t day;
    func__T_CapFixture(0u,path,day);unsigned index=0;
    while(func__T_CodecFileSize(path)+SimpleLogCodec::MaxFrame<=SIMPLE_LOG_DAY_MAX_BYTES)
        if(!func__SimpleLog_CodecAppend(path,func__T_ByteFixtureRow(++index).c_str()))abort();
    const std::string before=func__T_ChartBytes(path),incoming=func__T_ByteFixtureRow(index);
    for(bool cached:{true,false})
    {
        func__T_ChartWrite(path,before);
        // Restore the real last writer state after restoring the corresponding original bytes.
        BOOL__G__LogCodecCached=false;
        if(cached){
            File f=LittleFS.open(path,"r");SimpleLogCodec::begin(f,SIMPLE_LOG_CODEC_WRITE_STATE);
            while(SimpleLogCodec::next(f,SIMPLE_LOG_CODEC_WRITE_STATE)){}
            snprintf(CHAR__G__LogCodecPath,sizeof(CHAR__G__LogCodecPath),"%s",path);
            UINT32_T__G__LogCodecSize=(uint32_t)before.size();BOOL__G__LogCodecCached=true;
        }
        BOOL__T__CredentialOpenFail=true;BOOL__T__CredentialRenameFail=true;
        bool ok=func__SimpleLog_AppendRow(day,incoming.c_str());
        BOOL__T__CredentialOpenFail=false;BOOL__T__CredentialRenameFail=false;
        const std::string after=func__T_ChartBytes(path);
        func__T_Check(ok&&after.size()==before.size()+9u&&after.compare(0u,before.size(),before)==0&&
            func__SimpleLog_CountRows(path)==index+1u,
            "a fitting nine-byte frame appends without a temporary file or rename, with or without codec cache");
    }
    func__T_CapFixture(0u,path,day);
    bool sizes=true;
    for(unsigned i=0u;i<40u;i++)
    {
        char row[SIMPLE_LOG_RAM_ROW_MAX];func__SimpleLog_BuildRow(row,sizeof(row),0u,0u);
        uint32_t values[SimpleLogCodec::Fields],predicted=0u;SimpleLogCodec::parse(row,values);
        if(i%2u)BOOL__G__LogCodecCached=false;
        sizes=sizes&&func__SimpleLog_AppendBytes(path,values,&predicted)&&func__SimpleLog_CodecAppend(path,row)&&
            func__T_CodecFileSize(path)==predicted;
    }
    func__T_Check(sizes,"size preview matches actual full/delta frames across repeated 16-record checkpoints");
}
static void func__T_GapPending(uint32_t base)
{
    LittleFS.format();func__T_BatteryBoot();func__T_ClockGapWait(40000u);
    func__Simple_TimeSet(base,true);func__SimpleLog_ResolveDay(true);BOOL__G__LogClockRetryArmed=false;
}
static void func__T_TestClockGapDeliveryRetry(void)
{
    const uint32_t base=1793836800u;
    func__T_GapPending(base);
    const std::string undated=func__T_ChartBytes(SIMPLE_TIME_GAP_FILE);
    BOOL__T__CredentialRenameFail=true;func__SimpleLog_ClockGapTick();BOOL__T__CredentialRenameFail=false;
    func__T_Check(func__T_ClockWarningRows()==0u&&func__T_ChartBytes(SIMPLE_TIME_GAP_FILE)==undated,
        "failed intent staging cannot append a warning or destroy the original pending incident");
    BOOL__G__LogClockRetryArmed=false;BOOL__T__RemoveFail=true;func__SimpleLog_ClockGapTick();BOOL__T__RemoveFail=false;
    const std::string staged=func__T_ChartBytes(SIMPLE_TIME_GAP_FILE);
    func__T_Check(func__T_ClockWarningRows()==1u&&staged.find("\"row\":\"")!=std::string::npos,
        "a delivered warning keeps its exact staged identity when marker removal fails");
    func__T_BatteryBoot();func__Simple_TimeSet(base+86400u,true);INT32_T__G__LogTzSec=12600;
    BOOL__G__LogClockRetryArmed=false;func__SimpleLog_ClockGapTick();
    func__T_Check(func__T_ClockWarningRows()==1u&&!LittleFS.exists(SIMPLE_TIME_GAP_FILE),
        "reboot on another day/timezone recognizes the earlier delivered incident and does not duplicate it");
    // A genuinely later incident must not be hidden just because an earlier warning exists.
    func__T_BatteryBoot();func__T_ClockGapWait(40000u);func__Simple_TimeSet(base+86460u,true);
    BOOL__G__LogClockRetryArmed=false;func__SimpleLog_ClockGapTick();
    func__T_Check(func__T_ClockWarningRows()==2u&&!LittleFS.exists(SIMPLE_TIME_GAP_FILE),
        "a new clock-loss incident still produces its own warning after an earlier one was completed");
    func__T_GapPending(base);
    SIM_SIZE_T__G__WriteLimit=0u;func__SimpleLog_ClockGapTick();SIM_SIZE_T__G__WriteLimit=(size_t)-1;
    char pending[SIMPLE_LOG_RAM_ROW_MAX];uint32_t pendingDay=0u;
    func__T_Check(func__SimpleLog_ClockGapIntent(pending,&pendingDay)==1&&func__T_ClockWarningRows()==0u,
        "binary append failure retains the staged row before any dated warning exists");
    func__T_BatteryBoot();func__Simple_TimeSet(base+60u,true);BOOL__G__LogClockRetryArmed=false;
    func__SimpleLog_ClockGapTick();uint32_t warningEpoch=0u;
    func__T_Check(func__T_ClockWarningRows(&warningEpoch)==1u&&warningEpoch==base&&!LittleFS.exists(SIMPLE_TIME_GAP_FILE),
        "reboot before delivery appends the original staged row exactly once instead of inventing another timestamp");
    func__T_GapPending(base);func__T_ChartWrite(SIMPLE_TIME_GAP_FILE,"malformed intent");
    func__SimpleLog_ClockGapTick();
    func__T_Check(func__T_ClockWarningRows()==0u&&func__T_ChartBytes(SIMPLE_TIME_GAP_FILE)=="malformed intent",
        "unreadable intent format is retained for diagnosis rather than guessed or appended again");
}
static void func__T_TestLogRetryDelivery(void)
{
    printf("\n== audited day retry, flash-write reduction and warning delivery ==\n");
    func__T_TestDayRetryInitialization();func__T_TestExactAppendSizing();func__T_TestClockGapDeliveryRetry();
}
