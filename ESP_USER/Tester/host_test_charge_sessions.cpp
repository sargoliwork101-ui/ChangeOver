/* Test the real ESP charge observer, snapshot codec, UART and HTTP paths. */
#define main func__T_UnusedSimulatorMain
#include "../Simulator/main.cpp"
#undef main
#include <cassert>
#include <cstdio>
static unsigned checks = 0u;
static void check(bool ok, const char *message)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); abort(); }
    checks++;
    printf("PASS: %s\n", message);
}
static void frame(uint32_t first, uint32_t second, unsigned elapsed = 500u)
{
    UNSIGNED_LONG__G__MillisStart -= elapsed;
    UINT32_T__G__TlmField[6] = first;
    UINT32_T__G__TlmField[13] = second;
    func__Simple_ChargeOnFrame();
}
static void clean(void)
{
    func__Simple_ChargeHistoryClear(false);
    BOOL__G__TimeFromDisk = false;
    INT32_T__G__LogTzSec = 0;
    func__Simple_TimeSet(1793836800u, true);
}
int main()
{
    setenv("SIMPLE_SIM_FS", "/tmp/changeover_charge_sessions_fs", 1);
    setenv("SIMPLE_SIM_PORT", "0", 1);
    setup();LittleFS.format();clean();
    frame(0,0);frame(1,2);
    check(UINT8_T__G__ChargeHistoryCount == 2u, "two channels start independent sessions");
    uint32_t start = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].startSec;
    check(start >= SIMPLE_TIME_VALID_MIN && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[1].startSec == start, "observed inactive-to-pumping edge records start time");
    frame(2,1);frame(2,1);frame(3,6);
    check(UINT8_T__G__ChargeHistoryCount == 2u, "BULK/ABSORB changes never duplicate a session");
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].status == 1u && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[1].status == 2u, "FLOAT completes and input-wait interrupts without touching board counters");
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].durationSec >= 1u && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].durationSec <= 2u, "duration counts fresh pumping milliseconds");
    frame(3,6);frame(3,6);check(UINT8_T__G__ChargeHistoryCount == 2u,"repeated terminal telemetry never adds another event");
    clean();frame(0,0);frame(1,0);frame(1,0);frame(5,0);
    uint32_t before = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].durationSec;
    frame(5,0);frame(10,0);frame(4,0);frame(2,0);frame(3,0);
    check(UINT8_T__G__ChargeHistoryCount == 1u && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].status == 1u,"retry, bringup and thermal pauses preserve the same session");
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].durationSec <= before + 1u,"paused intervals do not count as charging time");
    clean();frame(1,0);frame(2,0);frame(3,0);
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].startSec == 0u && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].durationSec == 0u,"starting observation mid-charge never invents start or full duration");
    clean();frame(0,0);frame(1,0);frame(1,0);uint32_t last = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].timestamp;
    UNSIGNED_LONG__G__MillisStart -= 2000u;func__Simple_ChargeTick();
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].status == 0u && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].timestamp == last && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].durationSec == 0u,"stale UART seals last observation as unknown, not interrupted or completed");
    frame(1,0);check(UINT8_T__G__ChargeHistoryCount == 2u && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[1].startSec == 0u,"resuming after a gap does not merge unobserved charging");
    clean();frame(0,0);frame(1,0);frame(1,0,2000u);
    check(UINT8_T__G__ChargeHistoryCount == 2u && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].status == 0u,"a delayed frame detects loss even before the watchdog runs");
    clean();frame(0,0);frame(1,0);func__Simple_TimeSet(1793846800u,true);frame(1,0);
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].status == 0u && SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[1].startSec == 0u,"clock corrections never become charging duration");
    clean();BOOL__G__TimeValid=false;frame(1,0);check(UINT8_T__G__ChargeHistoryCount==0u,"unconfirmed clock cannot create dated sessions");
    func__Simple_TimeSet(1793836800u,true);frame(1,0);check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].startSec==0u,"clock recovery mid-charge does not fabricate the missing start");
    clean();frame(0,0);frame(1,0);frame(7,0);frame(1,0);frame(8,0);frame(1,0);frame(0,0);
    check(UINT8_T__G__ChargeHistoryCount==3u,"fault, removal and off each finish one interrupted session");
    for(unsigned i=0;i<3;i++)check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[i].status==2u,"interrupted sessions are never complete");
    clean();frame(0,0);frame(1,0);frame(1,0);frame(3,0);frame(0,0);frame(0,1);frame(0,1);
    func__Simple_ChargeHistorySave();check(!BOOL__G__ChargeHistDirty,"new session snapshot commits normally");
    const uint32_t savedStart=SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].startSec;
    func__Simple_ChargeHistoryLoad();check(UINT8_T__G__ChargeHistoryCount==2u&&SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].startSec==savedStart&&SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].status==1u,"start, duration and completed status survive snapshot reload");
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[1].status==0u&&BOOL__G__ChargeHistDirty,"persisted live session is unknown after reboot, never falsely live");
    File previous=LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE,"r");String bytes=previous.readString();previous.close();
    SIM_BOOL__G__RefuseWrites=true;func__Simple_ChargeHistorySave();SIM_BOOL__G__RefuseWrites=false;
    File retained=LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE,"r");String after=retained.readString();retained.close();
    check(after==bytes&&BOOL__G__ChargeHistDirty,"failed snapshot preserves committed data and dirty retry");
    func__Simple_ChargeHistorySave();check(!BOOL__G__ChargeHistDirty,"snapshot retries after storage recovers");
    func__Simple_ChargeHistoryClear(true);func__Simple_ChargeHistoryLoad();check(!UINT8_T__G__ChargeHistoryCount,"erased session history stays erased after reboot");
    frame(1,0);check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].startSec==0u,"clear during charging restarts observation with unknown start");
    const char *bad[]={"{\"v\":2,\"hist\":[[1793836800,1,60,1793836740,1]", "{\"v\":2,\"hist\":[[1793836800,3,60,0,1]]}", "{\"v\":2,\"hist\":[[42949672960,1,60,0,1]]}", "{\"v\":2,\"hist\":[[1793836800,1,60,1793836900,1]]}", "{\"v\":2,\"hist\":[[1793836800,1,60,0,1],]}","{\"count\":1,\"hist\":[{\"ts\":1793836800,\"ch\":1}]}"};
    for(const char *text:bad){File f=LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE,"w");f.print(text);f.close();func__Simple_ChargeHistoryLoad();check(!UINT8_T__G__ChargeHistoryCount,"malformed, truncated or obsolete snapshot cannot manufacture records");}
    clean();frame(0,1);
    for(unsigned i=0;i<25;i++){frame(1,1,100u);frame(3,1,100u);}
    check(UINT8_T__G__ChargeHistoryCount==SIMPLE_DIAG_MAX_POWER_EVENTS&&func__Simple_ChargeFind(1u)>=0,"history cap evicts oldest finished events without discarding the other live channel");
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[1].timestamp<=SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount-1].timestamp,"bounded history retains newest finished observations");
    clean();uint32_t now=func__Simple_NowSec();INT32_T__G__LogTzSec=12600;
    int64_t cutoff=(((int64_t)now+12600)/86400-29)*86400-12600;
    UINT8_T__G__ChargeHistoryCount=2;SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0]={(uint32_t)cutoff-1u,1u,0u,0u,1u};SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[1]={(uint32_t)cutoff,2u,0u,0u,1u};
    BOOL__G__TimeFromDisk=true;BOOL__G__TimeBoardSeen=false;func__Simple_ChargePrune(now);check(UINT8_T__G__ChargeHistoryCount==2u,"unconfirmed disk clock cannot prune records");
    BOOL__G__TimeFromDisk=false;func__Simple_ChargePrune(now);check(UINT8_T__G__ChargeHistoryCount==1u&&SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].timestamp==(uint32_t)cutoff,"retention includes exact first local midnight and removes only expired dates");
    clean();UINT32_T__G__ChargeCountCh1=9u;frame(0,0);frame(1,0);frame(3,0);check(UINT32_T__G__ChargeCountCh1==9u,"observer never changes the board's lifetime completion counters");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/diag",HTTP_GET);const std::string body=SIMPLE_WEB_SERVER_T__G__Server.simBody();
    check(body.find("\"status\":\"complete\"")!=std::string::npos&&body.find("\"start\":")!=std::string::npos,"public diagnostic JSON includes real session status and start");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/calendar.js",HTTP_GET);check(SIMPLE_WEB_SERVER_T__G__Server.simBody()==SIMPLE_CHARGE_CALENDAR_JS,"calendar URL serves exactly the production flash asset");
    clean();uint8_t payload[SIMPLE_LINK_TLM_SIZE]={};func__Sim_PutU32(&payload[4u+6u*4u],0u);func__Sim_PutU32(&payload[4u+13u*4u],0u);
    func__Sim_SendFrame(SIMPLE_MSG_TLM_LIVE,payload,sizeof(payload));func__Simple_DrainSerial();
    func__Sim_PutU32(&payload[4u+6u*4u],1u);func__Sim_SendFrame(SIMPLE_MSG_TLM_LIVE,payload,sizeof(payload));func__Simple_DrainSerial();
    check(UINT8_T__G__ChargeHistoryCount==1u&&SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].status==3u,"real CRC-framed UART telemetry reaches the session observer");
    // A counter increase is not a timestamped session, and a reboot invalidates an open observation.
    clean();uint8_t diag[39]={};diag[0]=0u;func__Sim_PutU32(&diag[13],4u);func__Sim_PutU32(&diag[23],10u);
    func__Sim_SendFrame(SIMPLE_MSG_DIAG_DATA,diag,sizeof(diag));func__Simple_DrainSerial();
    func__Sim_PutU32(&diag[23],11u);func__Sim_SendFrame(SIMPLE_MSG_DIAG_DATA,diag,sizeof(diag));func__Simple_DrainSerial();
    check(!UINT8_T__G__ChargeHistoryCount&&UINT32_T__G__ChargeCountCh1==11u,"lifetime counter increments cannot fabricate dated completions");
    func__Simple_TimeSet(1793836800u,true);frame(0,0);frame(1,0);
    func__Sim_PutU32(&diag[13],5u);func__Sim_SendFrame(SIMPLE_MSG_DIAG_DATA,diag,sizeof(diag));func__Simple_DrainSerial();
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].status==0u,"confirmed board reset seals the previous live session as unknown");
    clean();frame(99,0);frame(1,0);check(!SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].startSec,"invalid prior charger state cannot establish a start edge");
    clean();frame(5,0);frame(1,0);check(!SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].startSec,"first observation in retry wait does not invent an earlier session start");
    clean();frame(0,0);frame(4,0);frame(1,0);check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0].startSec>=SIMPLE_TIME_VALID_MIN,"an observed inactive edge stays armed through bringup before pumping");
    printf("ALL %u REAL CHARGE SESSION CHECKS PASSED\n", checks);
}
