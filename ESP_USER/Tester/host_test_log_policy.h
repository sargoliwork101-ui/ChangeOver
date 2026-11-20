/* Current log schema, day markers and limits through production paths. */
static unsigned func__T_LogNoteCount(const char *path, unsigned note)
{
    File file=LittleFS.open(path,"r");SimpleLogCodec::State state;unsigned count=0;
    if(file && SimpleLogCodec::begin(file,state)==1)
        while(SimpleLogCodec::next(file,state))if(state.value[3]==note)++count;
    return count;
}
static void func__T_TestLogPolicy(void)
{
    printf("\n== 30-day / 200-row / 18-column log policy ==\n");
    LittleFS.format();func__T_BatteryResetRam();
    const uint32_t base=1793836800u;
    func__Simple_TimeSet(base,true);
    UINT8_T__G__TlmFlags=0x3fu;UINT8_T__G__TlmFlags2=18u;
    for(unsigned i=0;i<31u;i++)UINT32_T__G__TlmField[i]=1000u+i;
    UINT32_T__G__TlmField[17]=12001u;UINT32_T__G__TlmField[18]=12999u;
    UINT32_T__G__TlmField[15]=25000u;
    char row[256];uint32_t fields[SimpleLogCodec::Fields]={};
    func__SimpleLog_BuildRow(row,sizeof(row),512u,0u);
    const uint32_t expected[]={base,(uint32_t)(millis()/1000u),512u,0u,1u,0x3du,
        1006u,1013u,1019u,18u,12001u,12999u,1028u,1014u,1003u,1010u,1005u,1012u};
    bool same=SimpleLogCodec::parse(row,fields);
    for(unsigned i=0;i<18u;i++)if(fields[i]!=expected[i])same=false;
    func__T_Check(same,"all 18 stored columns match calibration order and masked flags exactly");
    func__T_Check(SIMPLE_LOG_MAX_DAYS==30u && SIMPLE_LOG_MAX_ROWS_PER_DAY==200u &&
        SIMPLE_CHART_3M_SIZE==90u && SIMPLE_LOG_TOTAL_BUDGET_BYTES==28672u,
        "log limits change without changing 90-day charts or the memory share");
    const uint32_t day=func__SimpleLog_TodayCode();char path[SIMPLE_LOG_PATH_MAX];func__SimpleLog_PathOf(path,day);
    UINT32_T__G__LogLastBeatSec=base-100u;UINT32_T__G__LogPrevV24=24700u;
    SIM_BOOL__G__RefuseWrites=true;
    func__T_Check(!func__SimpleLog_PrepareDayFile(day),"refused day-marker write is not reported as success");
    SIM_BOOL__G__RefuseWrites=false;
    func__T_Check(func__SimpleLog_ResolveDay(true) && func__T_LogNoteCount(path,7u)==1u &&
        func__SimpleLog_CountRows(path)==0u,"new day starts with exactly one marker outside the 200 event slots");
    func__T_Check(UINT32_T__G__LogLastBeatSec==base-100u && UINT32_T__G__LogPrevV24==24700u &&
        !BOOL__G__LogLastRowValid,"day marker does not consume voltage or hourly timing baselines");
    for(unsigned i=0;i<10u;i++)func__SimpleLog_PrepareDayFile(day);
    func__T_Check(func__T_LogNoteCount(path,7u)==1u,"opening an existing day never duplicates its start marker");
    func__SimpleLog_WriteRow(0u,0u);
    const uint32_t beat=UINT32_T__G__LogLastBeatSec;
    BOOL__G__LogHasPrev=false;BOOL__G__LogScanValid=false;func__SimpleLog_ScanDays(true);func__SimpleLog_RestoreFromLastRow();
    func__T_Check(BOOL__G__LogRestoredPrev && UINT32_T__G__LogPrevV24==25000u &&
        UINT32_T__G__LogPrevDead==1028u && UINT32_T__G__LogLastBeatSec==beat,
        "18-column reboot recovery sums halves and restores the correct dead-mask column");
    for(unsigned i=0;i<210u;i++)func__SimpleLog_WriteRow(0u,0u);
    func__T_Check(UINT16_T__G__LogRowsToday==200u && func__SimpleLog_CountRows(path)==200u &&
        BOOL__G__LogDayFull && !func__SimpleLog_WriteRow(0u,0u) &&
        func__T_LogNoteCount(path,4u)==1u,"200-row cap stops ordinary writes with one day-full marker");
    BOOL__G__LogDayFileReady=false;UINT32_T__G__LogDayCode=SIMPLE_LOG_DAY_AUTO;
    func__SimpleLog_ResolveDay(true);
    func__T_Check(UINT16_T__G__LogRowsToday==200u && BOOL__G__LogDayFull &&
        func__T_LogNoteCount(path,7u)==1u,"reopening a full day preserves its cap and single start marker");
    func__Simple_TimeSet(base+86400u,true);func__SimpleLog_ResolveDay(true);
    char next[SIMPLE_LOG_PATH_MAX];func__SimpleLog_PathOf(next,func__SimpleLog_TodayCode());
    func__T_Check(UINT16_T__G__LogRowsToday==0u && !BOOL__G__LogDayFull &&
        func__T_LogNoteCount(next,7u)==1u,"midnight opens a fresh marked day and clears yesterday's cap");
    // Verify an interrupted first marker can be repaired and retried exactly once.
    LittleFS.format();func__T_BatteryResetRam();func__Simple_TimeSet(base,true);
    SIM_SIZE_T__G__WriteLimit=SimpleLogCodec::HeaderSize;
    func__SimpleLog_PrepareDayFile(day);SIM_SIZE_T__G__WriteLimit=(size_t)-1;
    func__T_Check(func__SimpleLog_PrepareDayFile(day) && func__T_LogNoteCount(path,7u)==1u,
        "partial day-marker append recovers to one complete marker");
    uint32_t widest[SimpleLogCodec::Fields];for(unsigned i=0;i<18u;i++)widest[i]=UINT32_MAX;
    SimpleLogCodec::State wide;for(unsigned i=0;i<18u;i++)wide.value[i]=widest[i];
    char wideRow[SIMPLE_LOG_RAM_ROW_MAX];
    func__T_Check(SimpleLogCodec::text(wide,wideRow,sizeof(wideRow))>0u &&
        SimpleLogCodec::parse(wideRow,fields),"RAM row buffer holds even eighteen UINT32_MAX columns intact");
}
