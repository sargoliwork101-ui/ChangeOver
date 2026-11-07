/* [EN] v1.139 audit regressions: non-destructive reads, FIFO retries and bounded day tails. */
static void test_log_audit()
{
    LittleFS.files.clear(); reset_clock_test_boot(); func__EspLog_ResetCaches();
    LittleFS.failOpenCount=0; LittleFS.writeLimit=SIZE_MAX;
    const uint32_t epoch=1794000000u;
    BOOL__G__LogClockValid=true; UINT32_T__G__LogAnchorSec=epoch;
    UINT32_T__G__LogAnchorMs=0u; INT32_T__G__LogTzSec=0;
    const uint32_t day=func__EspLog_TodayCode();
    char path[ESP_LOG_PATH_MAX]; func__EspLog_PathOf(path,day);
    std::vector<std::string> rows;
    for (uint32_t r=0;r<8;++r) {
        EspLogCodec::State st;
        uint32_t values[18]={epoch+r,r,2u,0u,1u,1u,2u,2u,0u,0u,12000u+r,12100u,0u,24000u,500u,510u,400u,410u};
        memcpy(st.value,values,sizeof values);
        char row[ESP_LOG_ROW_MAX]; EspLogCodec::text(st,row,sizeof row);
        rows.emplace_back(row);
        (void)func__EspLog_AppendRow(day,row);
    }
    const std::string original=LittleFS.files[path];
    bool headerSafe=true, repairSafe=true, bodySafe=true;
    for (size_t limit=0;limit<8;++limit) {
        LittleFS.files[path]=original; BOOL__G__LogCodecCached=false;
        LittleFS.readLimit=limit;
        headerSafe &= !func__EspLog_AppendRow(day,rows.back().c_str()) && LittleFS.files[path]==original;
        LittleFS.files[path]=original;
        repairSafe &= !func__EspLog_RepairTail(path) && LittleFS.files[path]==original;
    }
    LittleFS.readLimit=SIZE_MAX;
    check(headerSafe,"audit: short header READ is never mistaken for an interrupted WRITE or erased");
    check(repairSafe,"audit: repair refuses short header reads without truncating a healthy file");
    for (size_t failAt=8;failAt<original.size();++failAt) {
        LittleFS.files[path]=original; BOOL__G__LogCodecCached=false;
        LittleFS.readFailAt=failAt;
        bodySafe &= !func__EspLog_AppendRow(day,rows.back().c_str()) && LittleFS.files[path]==original;
        LittleFS.files[path]=original;
        bodySafe &= !func__EspLog_RepairTail(path) && LittleFS.files[path]==original;
    }
    LittleFS.readFailAt=SIZE_MAX;
    check(bodySafe,"audit: every body short-read position aborts append/repair, preserving all original bytes");
    LittleFS.files[path]=original; BOOL__G__LogCodecCached=false;
    LittleFS.readFailAt=original.size()-1;
    ESP_WEB_SERVER_T__G__Server.clearArgs();
    const std::string dayArg=std::to_string(day);
    ESP_WEB_SERVER_T__G__Server.addArg("day",dayArg.c_str());
    func__Esp_HttpEventDownload();
    check(ESP_WEB_SERVER_T__G__Server.lastBody.find("# WARNING:")!=std::string::npos,
          "audit: incomplete read produces an explicit CSV export warning");
    func__EspLog_LoadLastSnapshot();
    check(!BOOL__G__LogBootSnapValid,"audit: short body read cannot masquerade as the latest boot snapshot");
    UINT32_T__G__LogDayCode=ESP_LOG_DAY_AUTO;
    check(!func__EspLog_OpenDay() && UINT32_T__G__LogDayCode!=day,
          "audit: failed repair/recount does not cache a successfully opened day");
    LittleFS.readFailAt=SIZE_MAX;
    check(func__EspLog_OpenDay() && UINT16_T__G__LogRowsToday==8u,
          "audit: healthy day-open retry recounts the complete file after a read error");
    check(func__EspLog_AppendRow(day,rows.back().c_str()),"audit: a healthy retry succeeds after transient read failure");

    /* [EN] A refused drain followed by a healthy append must not reorder history. */
    LittleFS.files.clear(); func__EspLog_ResetCaches();
    (void)func__EspLog_AppendRow(day,rows[0].c_str());
    const std::string first=LittleFS.files[path];
    UINT32_T__G__LogDayCode=day; UINT16_T__G__LogRowsToday=1u;
    const std::string queued=rows[1].substr(0,rows[1].size()-1u);
    func__EspLog_RamPush(queued.c_str());
    UINT32_T__G__LogAnchorSec=epoch+2u;
    char current[ESP_LOG_ROW_MAX];
    (void)func__EspLog_BuildRow(current,sizeof current,ESP_LOG_CHG_MAINS,ESP_LOG_NOTE_MANUAL);
    LittleFS.failOpenCount=1;
    (void)func__EspLog_WriteRow(ESP_LOG_CHG_MAINS,ESP_LOG_NOTE_MANUAL);
    check(LittleFS.files[path]==first && UINT8_T__G__LogRamCount==2u,
          "audit: new row cannot bypass an older RAM row after one transient drain failure");
    check(UINT8_T__G__LogWriteFault && BOOL__G__LogDrainWait && UINT16_T__G__LogRowsToday==1u,
          "audit: pending FIFO keeps fault/retry status and does not count unwritten rows");
    func__EspLog_DrainRam();
    check(decoded_day(path)==std::string(ESP_LOG_HEADER)+rows[0]+rows[1]+current+"\n" && !UINT8_T__G__LogRamCount,
          "audit: healthy retry persists FIFO order exactly once");

    /* [EN] Pending rows are the newest rows, and each belongs to its own day. */
    LittleFS.files.clear(); func__EspLog_ResetCaches();
    for (unsigned i=0;i<5;++i) (void)func__EspLog_AppendRow(day,rows[i].c_str());
    for (unsigned i=5;i<7;++i) {
        const std::string row=rows[i].substr(0,rows[i].size()-1u); func__EspLog_RamPush(row.c_str());
    }
    UINT8_T__G__LogWriteFault=1;
    char tail[512];
    func__EspLog_Tail(tail,sizeof tail,day,2u);
    check(tail==rows[5]+rows[6],"audit: file plus pending rows obey one shared newest-N tail limit");
    char tiny[100]; func__EspLog_Tail(tiny,sizeof tiny,day,30u);
    check(tiny==rows[6],"audit: tight table byte budget never hides the latest pending row behind older flash rows");
    EspLogCodec::State yesterday;
    EspLogCodec::parse(rows[0].c_str(),yesterday.value); yesterday.value[0]-=86400u;
    char oldRow[ESP_LOG_ROW_MAX]; EspLogCodec::text(yesterday,oldRow,sizeof oldRow);
    const uint32_t oldDay=func__EspLog_DateCode(func__EspLog_LocalDay(epoch-86400u));
    char oldPath[ESP_LOG_PATH_MAX]; func__EspLog_PathOf(oldPath,oldDay);
    (void)func__EspLog_AppendRow(oldDay,oldRow);
    func__EspLog_Tail(tail,sizeof tail,oldDay,30u);
    check(std::string(tail)==oldRow,"audit: an older day never borrows today's pending RAM rows");
    /* [EN] Cross-midnight pending rows must still be shown under their own day. */
    func__EspLog_RamPush(std::string(oldRow,strlen(oldRow)-1).c_str());
    func__EspLog_Tail(tail,sizeof tail,oldDay,30u);
    check(std::string(tail)==std::string(oldRow)+oldRow,"audit: RAM day filtering retains matching older-day rows across midnight");
    LittleFS.files.erase(oldPath);
    func__EspLog_Tail(tail,sizeof tail,oldDay,30u);
    check(std::string(tail)==oldRow,"audit: missing day file uses only matching pending rows");
    BOOL__G__FsOk=false;
    func__EspLog_Tail(tail,sizeof tail,day,30u);
    check(tail==rows[5]+rows[6],"audit: offline explicit day selection also filters cross-midnight RAM rows");
    func__EspLog_Tail(tiny,sizeof tiny,day,30u);
    check(tiny==rows[6],"audit: RAM-only tight tail keeps the newest matching row");
    BOOL__G__FsOk=true;
    LittleFS.files.clear(); func__EspLog_ResetCaches();
}
