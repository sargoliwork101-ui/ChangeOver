/* @file host_test_log_row_cap.h
 * @brief Verify minimal oldest-row eviction during RAM drain using production storage.
 */
static void func__T_CapFixture(unsigned rows, char *path, uint32_t &day)
{
    LittleFS.format();func__T_BatteryResetRam();func__Simple_TimeSet(1793836800u,true);
    day=func__SimpleLog_TodayCode();func__SimpleLog_PathOf(path,day);
    if(!func__SimpleLog_ResolveDay(true))abort();
    for(unsigned i=1u;i<=rows;i++)
    {
        func__Simple_TimeSet(1793836800u+i,true);UINT32_T__G__TlmField[14]=24000u+i;
        char row[SIMPLE_LOG_RAM_ROW_MAX];func__SimpleLog_BuildRow(row,sizeof(row),0u,0u);
        if(!func__SimpleLog_CodecAppend(path,row))abort();
    }
    UINT16_T__G__LogRowsToday=(uint16_t)rows;
    BOOL__G__LogDayFull=rows>=SIMPLE_LOG_MAX_ROWS_PER_DAY;
}
static bool func__T_CapHasRange(const char *path, unsigned first, unsigned last)
{
    File file=LittleFS.open(path,"r");SimpleLogCodec::State state;
    if(!file||SimpleLogCodec::begin(file,state)!=1)return false;
    unsigned marker=0u,next=first;
    while(SimpleLogCodec::next(file,state))
    {
        if(state.value[3]==SIMPLE_LOG_NOTE_DAY_START){++marker;continue;}
        if(next>last || state.value[0]!=1793836800u+next || state.value[13]!=24000u+next || state.value[3]!=0u)return false;
        ++next;
    }
    return marker==1u && next==last+1u && !state.ioError && !state.damaged;
}
static void func__T_CapQueue(unsigned index)
{
    func__Simple_TimeSet(1793836800u+index,true);UINT32_T__G__TlmField[14]=24000u+index;
    char row[SIMPLE_LOG_RAM_ROW_MAX];func__SimpleLog_BuildRow(row,sizeof(row),0u,0u);
    func__SimpleLog_RamPush(row);
}
static void func__T_TestLogRowCap(void)
{
    printf("\n== minimal oldest-event eviction at the 200-row cap ==\n");
    char path[SIMPLE_LOG_PATH_MAX];uint32_t day;
    func__T_CapFixture(191u,path,day);
    // Reproduce the original fault: writes fail while 16 fresh events accumulate.
    SIM_BOOL__G__RefuseWrites=true;
    for(unsigned i=192u;i<=207u;i++)
    {
        func__Simple_TimeSet(1793836800u+i,true);UINT32_T__G__TlmField[14]=24000u+i;
        func__SimpleLog_WriteRow(0u,0u);
    }
    SIM_BOOL__G__RefuseWrites=false;
    func__T_Check(UINT8_T__G__LogRamCount==16u,"16 events wait in RAM behind 191 committed events");
    func__SimpleLog_DrainRam();
    func__T_Check(UINT8_T__G__LogRamCount==0u && UINT16_T__G__LogRowsToday==200u &&
        func__SimpleLog_CountRows(path)==200u && func__T_CapHasRange(path,8u,207u),
        "207 events retain exactly events 8 through 207 plus the original day-start marker");
    func__SimpleLog_DrainRam();BOOL__G__LogDayFileReady=false;UINT32_T__G__LogDayCode=SIMPLE_LOG_DAY_AUTO;
    func__SimpleLog_ResolveDay(true);
    func__T_Check(UINT16_T__G__LogRowsToday==200u && func__T_CapHasRange(path,8u,207u),
        "a second drain and reopening cannot duplicate events or delete another seven");
    func__T_CapFixture(206u,path,day);func__T_CapQueue(207u);
    const std::string before=func__T_ChartBytes(path);
    for(bool *fault:{&SIM_BOOL__G__RefuseWrites,&BOOL__T__CredentialOpenFail,
        &BOOL__T__CredentialReadFail,&BOOL__T__CredentialRenameFail})
    {
        *fault=true;func__SimpleLog_DrainRam();*fault=false;
        func__T_Check(func__T_ChartBytes(path)==before && UINT8_T__G__LogRamCount==1u && UINT16_T__G__LogRowsToday==206u,
            "failed capped replacement preserves all committed bytes, counters and the queued new event");
    }
    SIM_SIZE_T__G__WriteLimit=4u;func__SimpleLog_DrainRam();SIM_SIZE_T__G__WriteLimit=(size_t)-1;
    func__T_Check(func__T_ChartBytes(path)==before && UINT8_T__G__LogRamCount==1u,
        "a torn temporary write never deletes committed old events or consumes the queued row");
    func__SimpleLog_DrainRam();
    func__T_Check(UINT8_T__G__LogRamCount==0u && UINT16_T__G__LogRowsToday==200u && func__T_CapHasRange(path,8u,207u),
        "retry of a previously overfull file deletes exactly its seven oldest events in one replacement");
    // A queued event for yesterday must trim yesterday, not today's counter or rows.
    func__T_CapFixture(200u,path,day);func__T_CapQueue(201u);
    func__Simple_TimeSet(1793923200u,true);func__SimpleLog_ResolveDay(true);
    func__SimpleLog_DrainRam();
    func__T_Check(UINT16_T__G__LogRowsToday==0u && UINT8_T__G__LogRamCount==0u && func__T_CapHasRange(path,2u,201u),
        "draining yesterday removes only yesterday's oldest event and leaves today's count unchanged");
}
