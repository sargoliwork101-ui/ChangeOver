/* [EN] v1.140: explicit deletion is exact, selective, cache-safe and failure-aware. */
static void test_file_delete()
{
    LittleFS.files.clear(); reset_clock_test_boot(); func__EspLog_ResetCaches();
    LittleFS.failOpenCount=0; LittleFS.failRemoves=false;
    LittleFS.readFailAt=LittleFS.readLimit=LittleFS.writeLimit=SIZE_MAX;
    auto request=[&](const std::string &name) {
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.args["name"]=name;
        func__Esp_HttpEventClean();
        return ESP_WEB_SERVER_T__G__Server.lastCode;
    };
    LittleFS.files["/keep.dat"]="keep";
    LittleFS.files["/foreign.dat"]="old";
    check(request("/foreign.dat")==200 && !LittleFS.exists("/foreign.dat") && LittleFS.files["/keep.dat"]=="keep",
          "file delete: removes exactly the selected foreign file");
    check(request("/absent")==404 && LittleFS.files.size()==1u,"file delete: missing file reports 404 without bulk cleanup");
    bool safe=true;
    for (const char *name:{"","/","relative","/..","/.","/../keep.dat","/a/b","//keep.dat"})
        safe &= request(name)==400 && LittleFS.files["/keep.dat"]=="keep";
    safe &= request(std::string("/keep.dat\0ignored",17))==400;
    check(safe,"file delete: empty, traversal, nested, noncanonical and embedded-NUL names cannot fall through to bulk deletion");
    LittleFS.failRemoves=true;
    check(request("/keep.dat")==503 && LittleFS.exists("/keep.dat"),"file delete: actual remove failure is not reported as success");
    LittleFS.failRemoves=false; BOOL__G__FsOk=false;
    check(request("/keep.dat")==503 && LittleFS.exists("/keep.dat"),"file delete: unavailable filesystem is non-destructive");
    BOOL__G__FsOk=true;
    LittleFS.files[ESP_BENCHLOG_PATH]="bench";
    check(request(ESP_BENCHLOG_PATH)==200 && !LittleFS.exists(ESP_BENCHLOG_PATH) && LittleFS.exists("/keep.dat"),
          "file delete: user may explicitly delete the owned bench CSV too");
    LittleFS.files[ESP_LOG_PCT_PATH]="40\n"; UINT8_T__G__LogBudgetPercent=40u;
    check(request(ESP_LOG_PCT_PATH)==200 && UINT8_T__G__LogBudgetPercent==ESP_LOG_BUDGET_PERCENT,
          "file delete: deleting saved share restores default without silently recreating the file");
    LittleFS.files[ESP_LOG_CLOCK_PATH]="clock"; BOOL__G__LogClockSavedOnce=true;
    check(request(ESP_LOG_CLOCK_PATH)==200 && !BOOL__G__LogClockSavedOnce && !LittleFS.exists(ESP_LOG_CLOCK_PATH),
          "file delete: saved clock may be deleted while the next save can safely recreate it");
    BOOL__G__LogClockValid=true; UINT32_T__G__LogAnchorSec=1794000000u; UINT32_T__G__LogAnchorMs=(uint32_t)millis();
    INT32_T__G__LogTzSec=0;
    const uint32_t today=func__EspLog_TodayCode();
    const uint32_t oldDay=func__EspLog_DateCode(func__EspLog_LocalDay(1793913600u));
    char todayPath[ESP_LOG_PATH_MAX],oldPath[ESP_LOG_PATH_MAX];
    func__EspLog_PathOf(todayPath,today); func__EspLog_PathOf(oldPath,oldDay);
    char row[ESP_LOG_ROW_MAX];
    (void)func__EspLog_BuildRow(row,sizeof row,0u,ESP_LOG_NOTE_NONE);
    (void)func__EspLog_AppendRow(today,row);
    LittleFS.files[oldPath]="# old\n";
    UINT32_T__G__LogDayCode=today; UINT16_T__G__LogRowsToday=ESP_LOG_MAX_ROWS_PER_DAY;
    BOOL__G__LogDayFull=BOOL__G__LogMarkerDone=true;
    /* [EN] Start near the ring boundary and keep rows from a different day. */
    UINT8_T__G__LogRamHead=ESP_LOG_RAM_ROWS-1u;
    func__EspLog_RamPush(row);
    const std::string oldRow="1793913600"+std::string(row+10);
    func__EspLog_RamPush(oldRow.c_str()); func__EspLog_RamPush(row);
    check(request(todayPath)==200 && !LittleFS.exists(todayPath) && LittleFS.exists(oldPath) &&
          UINT8_T__G__LogRamCount==1u && !BOOL__G__LogCodecCached && !BOOL__G__LogScanValid &&
          UINT16_T__G__LogRowsToday==0u && !BOOL__G__LogDayFull && !BOOL__G__LogMarkerDone,
          "file delete: current-day cache/cap reset, only matching pending rows removed, wrapped FIFO preserved");
    func__EspLog_DrainRam();
    check(!LittleFS.exists(todayPath) && LittleFS.files[oldPath].find(oldRow)!=std::string::npos,
          "file delete: pending rows cannot resurrect the deleted day; other days still drain");
    check(func__EspLog_AppendRow(today,row) && func__EspLog_CountRows(todayPath)==1u,
          "file delete: later logging safely creates a new valid compressed day");
    LittleFS.files.clear();
    const std::string odd="/a\"'\\<x>.txt";
    LittleFS.files[odd]="odd";
    char json[ESP_JSON_BUFFER_SIZE];
    func__EspLog_FsStatusJson(json,sizeof json);
    check(std::string(json).find("/a\\\"'\\\\<x>.txt")!=std::string::npos,
          "file delete: inspection JSON escapes names losslessly instead of replacing their bytes");
    check(request(odd)==200 && LittleFS.files.empty(),"file delete: quotes and backslashes select the exact filename");
    for (unsigned i=0;i<60;++i) LittleFS.files["/file-"+std::to_string(100+i)+".dat"]="data";
    std::string all; bool pages=true;
    for (unsigned offset:{0u,24u,48u}) {
        ESP_WEB_SERVER_T__G__Server.clearArgs();
        ESP_WEB_SERVER_T__G__Server.args["offset"]=std::to_string(offset);
        func__Esp_HttpEventFsStatus();
        const std::string page=ESP_WEB_SERVER_T__G__Server.lastBody;
        pages &= page.find("\"offset\":"+std::to_string(offset))!=std::string::npos;
        pages &= std::count(page.begin(),page.end(),'{')==(offset==48u?13:25);
        all+=page;
    }
    for (unsigned i=0;i<60;++i) pages &= all.find("/file-"+std::to_string(100+i)+".dat")!=std::string::npos;
    check(pages,"file delete: paging exposes every file beyond the original 24-file cap");
    check(request("/file-159.dat")==200 && LittleFS.files.size()==59u,
          "file delete: files from later pages are individually deletable");
    LittleFS.files.clear(); func__EspLog_ResetCaches(); ESP_WEB_SERVER_T__G__Server.clearArgs();
}
