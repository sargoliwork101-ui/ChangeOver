/* @file host_test_log_recovery_retention.h
 * @brief Exercise torn day starts, calendar retention and minimal byte-limit eviction.
 */
static void func__T_TestDayStartCuts(void)
{
    printf("\n== day-start recovery at every byte boundary ==\n");
    char path[SIMPLE_LOG_PATH_MAX];uint32_t day;
    func__T_CapFixture(0u,path,day);
    const std::string marker=func__T_ChartBytes(path);
    bool all=true;
    for(size_t cut=0u;cut<marker.size();cut++)
    {
        LittleFS.format();func__T_BatteryResetRam();
        func__T_ChartWrite(path,marker.substr(0u,cut));
        func__Simple_TimeSet(1793836800u,true);
        bool recovered=func__SimpleLog_ResolveDay(true) && func__T_LogNoteCount(path,7u)==1u &&
            func__SimpleLog_CountRows(path)==0u;
        recovered=recovered && func__SimpleLog_WriteRow(0u,0u) && func__SimpleLog_PrepareDayFile(day) &&
            func__T_LogNoteCount(path,7u)==1u && func__SimpleLog_CountRows(path)==1u;
        if(!recovered)printf("day-start recovery failed at cut %zu/%zu\n",cut,marker.size());
        all=all&&recovered;
    }
    func__T_Check(all,"every torn header/first-marker boundary recovers to one marker before one ordinary event");
    func__T_CapFixture(0u,path,day);SIM_SIZE_T__G__WriteLimit=4u;
    // Reproduce the audited four-byte initial header, not just a truncated fixture.
    LittleFS.remove(path);BOOL__G__LogDayFileReady=false;
    bool first=func__SimpleLog_PrepareDayFile(day);SIM_SIZE_T__G__WriteLimit=(size_t)-1;
    func__T_Check(!first && func__SimpleLog_PrepareDayFile(day) && func__T_LogNoteCount(path,7u)==1u,
        "an actual four-byte first write retries with a complete header and one start marker");
}
static void func__T_TestCalendarRetention(void)
{
    printf("\n== verified local 30-calendar-day retention ==\n");
    char todayPath[SIMPLE_LOG_PATH_MAX];uint32_t today;
    func__T_CapFixture(0u,todayPath,today);
    const uint32_t base=1793836800u;
    char keep[SIMPLE_LOG_PATH_MAX],old[SIMPLE_LOG_PATH_MAX],ancient[SIMPLE_LOG_PATH_MAX];
    func__SimpleLog_PathOf(keep,func__SimpleLog_DateCode(base/86400u-29u));
    func__SimpleLog_PathOf(old,func__SimpleLog_DateCode(base/86400u-30u));
    func__SimpleLog_PathOf(ancient,func__SimpleLog_DateCode(base/86400u-60u));
    for(const char *p:{keep,old,ancient})func__T_ChartWrite(p,"historical fixture");
    const char *protectedPaths[]={SIMPLE_USERS_FILE,SIMPLE_WIFI_FILE,SIMPLE_CHARTS_FILE,SIMPLE_CHART_JOURNAL_FILE};
    for(const char *p:protectedPaths)func__T_ChartWrite(p,"protected bytes");
    BOOL__G__TimeFromDisk=true;BOOL__G__TimeBoardSeen=false;
    func__SimpleLog_ScanDays(true);func__SimpleLog_TrimDays();
    func__T_Check(LittleFS.exists(old)&&LittleFS.exists(ancient),"unverified restored clock cannot expire calendar history");
    BOOL__G__TimeFromDisk=false;
    BOOL__T__RemoveFail=true;func__SimpleLog_ScanDays(true);func__SimpleLog_TrimDays();BOOL__T__RemoveFail=false;
    func__T_Check(LittleFS.exists(ancient)&&UINT8_T__G__LogWriteFault>0u,"a refused expiration is reported and leaves the old file available for retry");
    func__SimpleLog_ScanDays(true);func__SimpleLog_TrimDays();
    func__T_Check(LittleFS.exists(keep)&&LittleFS.exists(todayPath)&&!LittleFS.exists(old)&&!LittleFS.exists(ancient),
        "with only four day files, age 29 is retained and ages 30 and 60 are removed");
    bool intact=true;for(const char *p:protectedPaths)intact=intact&&func__T_ChartBytes(p)=="protected bytes";
    func__T_Check(intact,"calendar expiry never modifies accounts, Wi-Fi or either chart file");
    char row[SIMPLE_LOG_RAM_ROW_MAX];func__Simple_TimeSet(base-60u*86400u,true);
    func__SimpleLog_BuildRow(row,sizeof(row),0u,0u);func__SimpleLog_RamPush(row);
    func__Simple_TimeSet(base,true);func__SimpleLog_DrainRam();
    func__T_Check(UINT8_T__G__LogRamCount==0u&&!LittleFS.exists(ancient),"an expired RAM event cannot recreate a deleted day file");
    // Local date, not UTC, controls the cutoff across midnight and year/leap boundaries.
    for(uint32_t stamp:{1709251199u,1735689599u,1793836799u})
    {
        func__Simple_TimeSet(stamp,true);INT32_T__G__LogTzSec=12600;
        uint32_t local=(stamp+12600u)/86400u;
        func__T_Check(!func__SimpleLog_DayExpired(func__SimpleLog_DateCode(local-29u))&&
            func__SimpleLog_DayExpired(func__SimpleLog_DateCode(local-30u)),
            "local midnight, leap-day and year boundaries retain exactly today and its preceding 29 days");
    }
    INT32_T__G__LogTzSec=0;
}
static std::string func__T_ByteFixtureRow(unsigned index)
{
    uint32_t values[SimpleLogCodec::Fields];
    // High-entropy but unsigned field values force measurable full-record byte pressure.
    uint32_t value=index*2654435761u;
    for(unsigned i=0u;i<SimpleLogCodec::Fields;i++){value=value*1664525u+1013904223u;values[i]=value;}
    values[0]=1793836800u+index;values[3]=0u;
    return func__T_CodecText(values);
}
static size_t func__T_ByteSuffixSize(const std::string &marker,unsigned first,unsigned last)
{
    SimpleLogCodec::State codec;uint8_t frame[SimpleLogCodec::MaxFrame];
    uint32_t values[SimpleLogCodec::Fields];size_t bytes=SimpleLogCodec::HeaderSize;
    SimpleLogCodec::parse(marker.c_str(),values);bytes+=SimpleLogCodec::encode(codec,values,frame);
    for(unsigned i=first;i<=last;i++)
    {
        SimpleLogCodec::parse(func__T_ByteFixtureRow(i).c_str(),values);
        bytes+=SimpleLogCodec::encode(codec,values,frame);
    }
    return bytes;
}
static void func__T_TestMinimalByteEviction(void)
{
    printf("\n== smallest fitting encoded suffix, not half-file deletion ==\n");
    char path[SIMPLE_LOG_PATH_MAX];uint32_t day;func__T_CapFixture(0u,path,day);
    const std::string marker=func__T_DayText(path);
    unsigned count=0u;
    while(func__T_ByteSuffixSize(marker,1u,count+1u)<=SIMPLE_LOG_DAY_MAX_BYTES)++count;
    for(unsigned i=1u;i<=count;i++)if(!func__SimpleLog_CodecAppend(path,func__T_ByteFixtureRow(i).c_str()))abort();
    UINT16_T__G__LogRowsToday=(uint16_t)count;
    unsigned first=1u;
    while(func__T_ByteSuffixSize(marker,first,count+1u)>SIMPLE_LOG_DAY_MAX_BYTES)++first;
    const std::string before=func__T_ChartBytes(path), incoming=func__T_ByteFixtureRow(count+1u);
    for(bool *fault:{&SIM_BOOL__G__RefuseWrites,&BOOL__T__CredentialOpenFail,&BOOL__T__CredentialReadFail,&BOOL__T__CredentialRenameFail})
    {
        *fault=true;bool ok=func__SimpleLog_AppendRow(day,incoming.c_str());*fault=false;
        func__T_Check(!ok&&func__T_ChartBytes(path)==before&&UINT16_T__G__LogRowsToday==count,
            "failed byte-limited replacement leaves every old event and its counter unchanged");
    }
    bool ok=func__SimpleLog_AppendRow(day,incoming.c_str());
    std::string expected=marker;
    for(unsigned i=first;i<=count+1u;i++)expected+=func__T_ByteFixtureRow(i)+"\n";
    func__T_Check(ok&&func__T_DayText(path)==expected&&func__SimpleLog_CountRows(path)==count+2u-first&&
        first>1u&&first-1u<count/2u&&func__T_CodecFileSize(path)<=SIMPLE_LOG_DAY_MAX_BYTES,
        "a full 4-KiB file drops only the minimum oldest events and retains every field of its newest suffix");
    func__T_Check(func__T_ByteSuffixSize(marker,first-1u,count+1u)>SIMPLE_LOG_DAY_MAX_BYTES,
        "keeping even one more old event would exceed 4 KiB after re-encoding");
    const std::string committed=func__T_ChartBytes(path);
    BOOL__G__LogCodecCached=false;func__SimpleLog_PrepareDayFile(day);
    func__T_Check(func__T_ChartBytes(path)==committed&&func__T_LogNoteCount(path,7u)==1u,
        "reopening the minimally compacted day preserves its bytes and single start marker");
}
static void func__T_TestLogRecoveryRetention(void)
{
    func__T_TestDayStartCuts();func__T_TestCalendarRetention();func__T_TestMinimalByteEviction();
}
