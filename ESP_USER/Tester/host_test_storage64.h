/* Shared quotas, checked snapshots and durable clears use production functions. */
static void func__T_TestStorage64(void)
{
    printf("\n== shared 64 KiB storage policy ==\n");
    BOOL__G__FsOk=true;LittleFS.format();func__T_BatteryResetRam();
    func__Simple_ChartsClear(false);func__Simple_TimeSet(1793836800u,true);
    UINT8_T__G__ChargeHistoryCount=1u;
    SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0]={1793836800u,1u,900u};
    UINT8_T__G__DiagErrorCount=1u;
    SIMPLE_DIAG_ERROR_T__G__Errors[0]={1793836800u,4u,28000u,25000u,12490u,12510u,1u,3u};
    UINT8_T__G__PowerEventCount=0u;
    void (*saves[])()={func__Simple_ParamsSave,func__Simple_ChargeHistorySave,func__Simple_DiagSave};
    bool *dirty[]={&BOOL__G__BoardParamsDirty,&BOOL__G__ChargeHistDirty,&BOOL__G__DiagDirty};
    const char *paths[]={SIMPLE_PARAMS_FILE,SIMPLE_CHARGE_HISTORY_FILE,SIMPLE_DIAG_FILE};
    bool *faults[]={&SIM_BOOL__G__RefuseWrites,&SIM_BOOL__G__ShortPrint,&BOOL__T__CredentialOpenFail,
        &BOOL__T__CredentialReadFail,&BOOL__T__CredentialRenameFail};
    for(unsigned k=0;k<3;k++){
        *dirty[k]=true;saves[k]();const std::string before=func__T_ChartBytes(paths[k]);
        func__T_Check(!before.empty()&&!*dirty[k],"snapshot commits normally");
        for(bool *fault:faults){
            *dirty[k]=true;*fault=true;saves[k]();*fault=false;
            func__T_Check(*dirty[k]&&func__T_ChartBytes(paths[k])==before,"failed snapshot preserves committed bytes and dirty retry");
        }
        BOOL__G__FsOk=false;saves[k]();BOOL__G__FsOk=true;
        func__T_Check(*dirty[k],"unmounted flash never consumes snapshot dirty state");
        saves[k]();func__T_Check(!*dirty[k],"snapshot retries successfully");
    }
    UINT8_T__G__ChargeHistoryCount=0;UINT8_T__G__DiagErrorCount=0;
    func__Simple_ChargeHistoryLoad();func__Simple_DiagLoad();
    func__T_Check(UINT8_T__G__ChargeHistoryCount==1u&&UINT8_T__G__DiagErrorCount==1u&&
        SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].durationSec==900u,"snapshots survive reloading");
    String assembled("prefix");
    func__T_Check(!func__Simple_StorageTextAppend(assembled,String())&&assembled=="prefix",
        "an unallocated JSON fragment cannot silently produce a committed partial snapshot");
    const std::string params=func__T_ChartBytes(SIMPLE_PARAMS_FILE);
    std::string oversize(SIMPLE_STORAGE_SETTINGS_BYTES+1u,'x');
    func__T_Check(!func__Simple_StorageWrite(SIMPLE_PARAMS_FILE,String(oversize))&&func__T_ChartBytes(SIMPLE_PARAMS_FILE)==params,
        "oversize settings rejected without damaging old data");
    func__T_Check(!func__Simple_StorageQuota(SIMPLE_CHARTS_FILE,SIMPLE_STORAGE_CHART_BYTES+1u)&&
        !func__Simple_StorageQuota(SIMPLE_DIAG_FILE,SIMPLE_STORAGE_HISTORY_BYTES+1u),"chart and history quotas enforced");
    size_t total=SIM_SIZE_T__G__FsTotalBytes;SIM_SIZE_T__G__FsTotalBytes=2097152u;
    func__T_Check(func__SimpleLog_FreeBytes()<=SIMPLE_STORAGE_TOTAL_BYTES,"a larger partition cannot bypass the 64 KiB envelope");
    SIM_SIZE_T__G__FsTotalBytes=total;
    func__Simple_AuthLogout();func__Simple_LoginClear();func__Simple_AuthDefaults();
    (void)func__Simple_AuthLogin("admin","admin");
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token",CHAR__G__SessionToken);
    SIMPLE_WEB_SERVER_T__G__Server.on("/clear",HTTP_POST,func__Simple_HttpClearHistory);
    SIM_BOOL__G__RefuseWrites=true;
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/clear",HTTP_POST,"type=diag");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==500&&UINT8_T__G__DiagErrorCount==1u,"failed diagnostic clear is not reported as success");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/clear",HTTP_POST,"type=charge");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==500&&UINT8_T__G__ChargeHistoryCount==1u,"failed charge clear preserves RAM");
    SIM_BOOL__G__RefuseWrites=false;
    func__Simple_ChartAddTimed(50u,24000u,25000u);
    uint32_t seq=UINT32_T__G__ChartSequence;SIM_SIZE_T__G__WriteLimit=0u;
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/clear",HTTP_POST,"type=charts");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==500&&UINT16_T__G__Chart1D_Count==1u&&UINT32_T__G__ChartSequence==seq,"failed chart clear preserves all published samples");
    SIM_SIZE_T__G__WriteLimit=(size_t)-1;
    BOOL__T__RemoveFail=true;
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/clear",HTTP_POST,"type=charts");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==200&&LittleFS.exists(SIMPLE_CHART_JOURNAL_FILE),"empty chart checkpoint can safely supersede an undeletable journal");
    BOOL__T__RemoveFail=false;func__Simple_ChartsLoad();
    func__T_Check(UINT16_T__G__Chart1D_Count==0u,"old journal cannot resurrect cleared chart after reboot");
    func__Simple_TimeSet(1793836900u,true);func__Simple_ChartAddTimed(50u,24000u,25000u);func__Simple_ChartsLoad();
    func__T_Check(UINT16_T__G__Chart1D_Count==1u,"new charts continue after durable clear");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/clear",HTTP_POST,"type=all");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==200,"clear all acknowledges all committed snapshots");
    UINT8_T__G__DiagErrorCount=0;UINT8_T__G__PowerEventCount=0;func__Simple_DiagLoad();func__Simple_ChargeHistoryLoad();func__Simple_ChartsLoad();
    func__T_Check(!UINT8_T__G__DiagErrorCount&&!UINT8_T__G__ChargeHistoryCount&&!UINT16_T__G__Chart1D_Count,"all cleared histories remain empty after reboot");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/clear",HTTP_POST,"type=typo");
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==400,"unknown clear selector never clears everything");
    // Create a large valid day through the codec, then inject compaction failures.
    LittleFS.format();BOOL__G__LogScanValid=false;
    char path[SIMPLE_LOG_PATH_MAX];const uint32_t day=func__SimpleLog_TodayCode();func__SimpleLog_PathOf(path,day);
    func__SimpleLog_PrepareDayFile(day);
    for(unsigned i=0;i<220u;i++){
        char row[256];func__Simple_TimeSet(1793836800u+i*60u,true);
        UINT32_T__G__TlmField[15]=22000u+i*20u;func__SimpleLog_BuildRow(row,sizeof(row),SIMPLE_LOG_CHG_VSTEP,0u);
        if (!func__SimpleLog_CodecAppend(path,row)) abort();
    }
    const std::string before=func__T_ChartBytes(path);
    for(bool *fault:{&SIM_BOOL__G__RefuseWrites,&BOOL__T__CredentialOpenFail,&BOOL__T__CredentialReadFail,&BOOL__T__CredentialRenameFail}){
        *fault=true;bool ok=func__SimpleLog_CompactDay(path,day);*fault=false;
        func__T_Check(!ok&&func__T_ChartBytes(path)==before,"failed day compaction preserves original committed rows");
    }
    func__T_Check(func__SimpleLog_CompactDay(path,day)&&func__SimpleLog_CountRows(path)==200u,"compaction removes only the 20 excess oldest events when all 200 fit");
    std::string newest=func__T_DayText(path);
    func__T_Check(newest.find("1793838000,")!=std::string::npos&&newest.find("1793837940,")==std::string::npos&&newest.find("1793849940,")!=std::string::npos,"compaction drops oldest rows, never newest rows");
    func__T_Check(func__T_DayText(path).find(",0,7,")!=std::string::npos,"day compaction preserves the original day-start marker");
    // No manual delete success on a failed filesystem remove.
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/clr",HTTP_POST,func__Simple_HttpEventClear);
    BOOL__T__RemoveFail=true;SIMPLE_WEB_SERVER_T__G__Server.simCall("/evt/clr",HTTP_POST);
    func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==500&&LittleFS.exists(path),"failed log deletion is reported honestly");
    BOOL__T__RemoveFail=false;
    func__Simple_AuthLogout();func__T_ClearCookie();
}
