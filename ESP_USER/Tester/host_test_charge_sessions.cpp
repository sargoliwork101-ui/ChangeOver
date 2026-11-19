/* Exercise production admission, missing-data reconnection, snapshot, UART and simulator counts. */
#define main func__T_UnusedSimulatorMain
#include "../Simulator/main.cpp"
#undef main
#include <cassert>
#include <cstdio>
#include <filesystem>
static unsigned checks = 0u;
static void check(bool ok, const char *message)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); abort(); }
    checks++; printf("PASS: %s\n", message);
}
static void frame(uint32_t first, uint32_t second, unsigned elapsed = 1000u)
{
    UNSIGNED_LONG__G__MillisStart -= elapsed;
    UINT32_T__G__TlmField[6] = first; UINT32_T__G__TlmField[13] = second;
    func__Simple_ChargeOnFrame();
}
static void run(uint32_t first, uint32_t second, unsigned seconds)
{
    for (unsigned i=0;i<seconds;i++) frame(first,second);
}
static void clean()
{
    func__Simple_ChargeHistoryClear(false); BOOL__G__TimeFromDisk=false;
    INT32_T__G__LogTzSec=0; func__Simple_TimeSet(1793836800u,true);
}
static simple_charge_event_t event(uint8_t channel)
{
    for(unsigned i=0;i<UINT8_T__G__ChargeHistoryCount;i++)
        if(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[i].channel==channel) return SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[i];
    fprintf(stderr,"missing channel %u\n",channel);abort();
}
int main()
{
    char root[]="/tmp/changeover-charge-sessions-XXXXXX";assert(mkdtemp(root));
    setenv("SIMPLE_SIM_FS",root,1);setenv("SIMPLE_SIM_PORT","0",1);setup();clean();
    frame(0,0);frame(1,2);const uint32_t firstStart=func__Simple_NowSec();
    check(!UINT8_T__G__ChargeHistoryCount,"brief starts stay outside calendar history");
    run(1,2,599);check(!UINT8_T__G__ChargeHistoryCount,"599 seconds cannot admit either channel");
    run(2,1,2);check(UINT8_T__G__ChargeHistoryCount==2u,"both channels qualify only after over ten observed minutes");
    check(event(1).startSec==firstStart&&event(2).startSec==firstStart,"admitted sessions retain the original start, not minute ten");
    check(event(1).durationSec>=601&&event(1).durationSec<=602,"admission includes all prior observed charging time");
    frame(3,6);check(event(1).status==1&&event(2).status==2,"qualified sessions retain complete and partial outcomes");
    frame(3,6);frame(3,6);check(UINT8_T__G__ChargeHistoryCount==2,"repeated terminal frames never duplicate history");
    clean();frame(0,0);
    for(unsigned i=0;i<5;i++){frame(1,1);run(1,1,3);frame(0,7);}
    check(!UINT8_T__G__ChargeHistoryCount,"five short attempts create no fake partial or unknown history");
    frame(1,0);frame(3,0);check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==1,"a real early FLOAT completion is retained before ten minutes");
    check(event(1).startSec!=0,"early completion retains its observed start");
    clean();frame(0,0);frame(1,0);frame(4,0);
    UINT64_T__G__ChargeElapsedMs[0]=600000u;
    frame(4,0);check(!UINT8_T__G__ChargeHistoryCount,"exactly ten pumping minutes is still tentative");
    UINT64_T__G__ChargeElapsedMs[0]++;
    frame(4,0);check(UINT8_T__G__ChargeHistoryCount==1,"one millisecond over ten minutes admits the paused candidate");
    for(uint32_t state:{0u,6u,7u,8u,9u,99u}){
        clean();frame(0,0);frame(1,0);run(1,0,601);frame(state,0);
        check(event(1).status==(state<=9u?2u:0u),"qualified off/input-wait/fault/removal/manual/invalid exits retain their correct outcome");
    }
    clean();frame(0,0);frame(1,0);run(1,0,500);frame(1,1);run(1,1,102);
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==3,"one channel qualifying cannot promote the other channel's short attempt");
    frame(6,6);check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==2,"independent terminal exits retain only the qualified partial session");
    clean();frame(0,0);frame(1,0);run(1,0,500);run(10,0,200);run(5,0,100);run(4,0,20);
    check(!UINT8_T__G__ChargeHistoryCount,"thermal/retry/bringup pauses do not qualify a pending session");
    run(2,0,102);check(UINT8_T__G__ChargeHistoryCount==1,"resumed real charging qualifies the same candidate");
    check(event(1).durationSec>=601&&event(1).durationSec<=604,"pause time is excluded from recorded duration");
    frame(3,0);check(event(1).status==1,"pauses preserve a later valid completion");
    clean();frame(0,0);frame(1,0);run(1,0,599);
    UNSIGNED_LONG__G__MillisStart-=90000u;func__Simple_ChargeTick();
    check(!UINT8_T__G__ChargeHistoryCount,"ninety seconds without telemetry do not admit a candidate");
    frame(1,0,0);check(!UINT8_T__G__ChargeHistoryCount,"returning data does not count the missing ninety seconds");
    run(1,0,2);check(UINT8_T__G__ChargeHistoryCount==1,"short link loss resumes the same pending candidate");
    check(event(1).durationSec>=601&&event(1).durationSec<=602,"duration excludes the entire unobserved gap");
    uint32_t originalStart=event(1).startSec,observed=event(1).durationSec;
    UNSIGNED_LONG__G__MillisStart-=119000u;func__Simple_ChargeTick();
    check(event(1).status==3,"short telemetry loss leaves the qualified session open");
    frame(2,0,0);check(UINT8_T__G__ChargeHistoryCount==1&&event(1).startSec==originalStart,"same channel resumes without a fake new start");
    check(event(1).durationSec==observed,"unobserved time is never included in charging duration");
    const uint32_t last=event(1).timestamp;
    func__Simple_ChargeHistorySave();
    for(unsigned i=0;i<240;i++){
        UNSIGNED_LONG__G__MillisStart-=60000u;func__Simple_ChargeTick();
    }
    check(event(1).status==3&&event(1).timestamp==last,"four-hour telemetry loss leaves the original session awaiting fresh state, not finished unknown");
    check(event(1).durationSec==observed&&!BOOL__G__ChargeHistDirty,"watchdog neither counts missing time nor dirties history with a fake ending");
    frame(2,0);
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).startSec==originalStart&&event(1).status==3,"returning ABSORB after hours continues the same qualified session");
    check(event(1).durationSec==observed,"the whole four-hour outage is excluded from duration");
    run(2,0,601);
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).startSec==originalStart,"over ten further pumping minutes after reconnect still cannot add a duplicate session");
    frame(3,0);frame(3,0);
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==1,"completion after reconnect updates the original row exactly once");
    clean();frame(0,0);frame(1,0);run(1,0,500);frame(1,0,3600000u);
    check(!UINT8_T__G__ChargeHistoryCount&&BOOL__G__ChargePending[0],"an hour-long missing interval does not qualify or discard a short candidate");
    run(1,0,102);check(UINT8_T__G__ChargeHistoryCount==1,"only fresh pumping after reconnection can finish the ten-minute qualification");
    check(event(1).durationSec>=602u&&event(1).durationSec<605u,"pending observed time survives disconnection without adding the missing hour");
    for(uint32_t state:{0u,3u,6u,7u,8u,9u}){
        clean();frame(0,0);frame(1,0);run(1,0,601);
        uint32_t start=event(1).startSec,duration=event(1).durationSec;
        frame(state,0,3600000u);frame(state,0);
        check(UINT8_T__G__ChargeHistoryCount==1&&event(1).startSec==start&&event(1).status==(state==3u?1u:2u),"first terminal frame after a long outage resolves the same session as complete or partial");
        check(event(1).durationSec==duration,"terminal reconnect frame never charges the unobserved hour");
    }
    for(uint32_t state:{4u,5u,10u}){
        clean();frame(0,0);frame(1,0);run(1,0,601);uint32_t duration=event(1).durationSec;
        frame(state,0,3600000u);run(state,0,30);frame(2,0);frame(3,0);
        check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==1&&event(1).durationSec<=duration+2u,"returning in bringup/retry/thermal pause preserves one session and excludes paused and missing time");
    }
    clean();frame(0,0);frame(1,0);run(1,0,3);frame(3,0,3600000u);
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==1&&event(1).durationSec<5u,"observed early completion after an hour reconnect is retained without inventing duration");
    clean();frame(0,0);frame(1,0);run(1,0,3);frame(6,0,3600000u);
    check(!UINT8_T__G__ChargeHistoryCount&&!BOOL__G__ChargePending[0],"an observed partial exit still discards a brief unqualified attempt after reconnect");
    clean();frame(0,0);frame(1,2);run(1,2,601);frame(3,6,3600000u);
    check(UINT8_T__G__ChargeHistoryCount==2&&event(1).status==1&&event(2).status==2,"both channel outcomes resolve independently from the same returning frame");
    clean();frame(0,0);frame(1,0);run(1,0,500);
    check(!BOOL__G__ChargeHistDirty,"pending observation never dirties persistent history");
    func__Simple_ChargeHistorySave();func__Simple_ChargeHistoryLoad();
    check(!UINT8_T__G__ChargeHistoryCount&&!BOOL__G__ChargePending[0],"periodic save and reboot never persist a sub-threshold candidate");
    frame(1,0);run(1,0,102);check(!UINT8_T__G__ChargeHistoryCount,"reboot discards tentative elapsed time");
    clean();frame(1,0);run(2,0,601);
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).startSec==0&&event(1).durationSec==0,"mid-charge boot can qualify but never invents original start or full duration");
    frame(3,0);check(event(1).status==1,"unknown original start does not discard a true completion");
    clean();frame(0,0);frame(1,0);run(1,0,601);func__Simple_TimeSet(1793846800u,true);frame(1,0);
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==0,"clock jump immediately seals a qualified observation rather than merging dates");
    clean();frame(0,0);frame(1,0);run(1,0,500);func__Simple_TimeSet(1793846800u,true);frame(1,0);run(1,0,102);
    check(!UINT8_T__G__ChargeHistoryCount,"clock jump discards an unqualified candidate without leaking its elapsed time");
    clean();frame(0,0);frame(1,0);run(1,0,601);func__Simple_TimeSet(1793836800u,true);frame(1,0);
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==0,"backward clock correction is also a hard observation break");
    clean();BOOL__G__TimeValid=false;frame(1,0);run(1,0,610);
    check(!UINT8_T__G__ChargeHistoryCount,"unknown clock cannot create dated records");
    func__Simple_TimeSet(1793836800u,true);frame(1,0);frame(3,0);
    check(event(1).startSec==0,"clock recovery does not invent the missing start");
    clean();frame(0,0);frame(1,0);run(1,0,601);frame(3,0);frame(0,1);run(0,1,601);
    func__Simple_ChargeHistorySave();check(!BOOL__G__ChargeHistDirty,"qualified snapshot persists normally");
    const uint32_t savedStart=event(1).startSec;
    func__Simple_ChargeHistoryLoad();
    check(UINT8_T__G__ChargeHistoryCount==2&&event(1).startSec==savedStart&&event(1).status==1,"completed history survives reload unchanged");
    check(event(2).status==0&&BOOL__G__ChargeHistDirty,"persisted open record is unknown after reboot, never falsely resumed");
    File f=LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE,"r");String before=f.readString();f.close();
    SIM_BOOL__G__RefuseWrites=true;func__Simple_ChargeHistorySave();SIM_BOOL__G__RefuseWrites=false;
    File again=LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE,"r");String after=again.readString();again.close();
    check(after==before&&BOOL__G__ChargeHistDirty,"failed snapshot retains committed history and retry state");
    func__Simple_ChargeHistorySave();check(!BOOL__G__ChargeHistDirty,"snapshot retries after storage recovers");
    func__Simple_ChargeHistoryClear(true);func__Simple_ChargeHistoryLoad();check(!UINT8_T__G__ChargeHistoryCount,"explicit clear remains durable");
    frame(1,0);frame(3,0);check(event(1).startSec==0,"clear during charging cannot invent a start");
    const char *bad[]={"{\"v\":2,\"hist\":[[1793836800,1,60,1793836740,1]","{\"v\":2,\"hist\":[[1793836800,3,60,0,1]]}","{\"v\":2,\"hist\":[[42949672960,1,60,0,1]]}","{\"v\":2,\"hist\":[[1793836800,1,60,1793836900,1]]}","{\"v\":2,\"hist\":[[1793836800,1,60,0,1],]}","{\"count\":1,\"hist\":[{\"ts\":1793836800,\"ch\":1}]}"};
    for(const char *text:bad){File f=LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE,"w");f.print(text);f.close();func__Simple_ChargeHistoryLoad();check(!UINT8_T__G__ChargeHistoryCount,"malformed/obsolete snapshots cannot manufacture history");}
    clean();frame(0,1);run(0,1,601);
    for(unsigned i=0;i<25;i++){frame(1,1,100);frame(3,1,100);}
    check(UINT8_T__G__ChargeHistoryCount==20&&func__Simple_ChargeFind(1)>=0,"eviction preserves the other qualified live channel");
    check(SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[1].timestamp<=SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount-1].timestamp,"capacity eviction retains the newest finished observations in order");
    clean();uint32_t now=func__Simple_NowSec();INT32_T__G__LogTzSec=12600;
    int64_t cutoff=(((int64_t)now+12600)/86400-29)*86400-12600;
    UINT8_T__G__ChargeHistoryCount=2;SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0]={(uint32_t)cutoff-1u,1,0,0,1};SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[1]={(uint32_t)cutoff,2,0,0,1};
    BOOL__G__TimeFromDisk=true;BOOL__G__TimeBoardSeen=false;func__Simple_ChargePrune(now);check(UINT8_T__G__ChargeHistoryCount==2,"unconfirmed clock cannot prune history");
    BOOL__G__TimeFromDisk=false;func__Simple_ChargePrune(now);check(UINT8_T__G__ChargeHistoryCount==1&&event(2).timestamp==(uint32_t)cutoff,"local thirty-day boundary is unchanged");
    clean();UINT32_T__G__ChargeCountCh1=9;frame(0,0);frame(1,0);frame(3,0);check(UINT32_T__G__ChargeCountCh1==9,"ESP observer never changes main-board completion counts");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/diag",HTTP_GET);std::string body=SIMPLE_WEB_SERVER_T__G__Server.simBody();
    check(body.find("\"status\":\"complete\"")!=std::string::npos&&body.find("\"start\":")!=std::string::npos,"diagnostic JSON retains existing session schema");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/calendar.js",HTTP_GET);check(SIMPLE_WEB_SERVER_T__G__Server.simBody()==SIMPLE_CHARGE_CALENDAR_JS,"shared calendar asset is unchanged");
    clean();uint8_t payload[SIMPLE_LINK_TLM_SIZE]={};func__Sim_SendFrame(SIMPLE_MSG_TLM_LIVE,payload,sizeof(payload));func__Simple_DrainSerial();
    func__Sim_PutU32(&payload[4+6*4],1);func__Sim_SendFrame(SIMPLE_MSG_TLM_LIVE,payload,sizeof(payload));func__Simple_DrainSerial();
    check(!UINT8_T__G__ChargeHistoryCount,"real CRC-framed UART start remains pending");
    func__Sim_PutU32(&payload[4+6*4],3);func__Sim_SendFrame(SIMPLE_MSG_TLM_LIVE,payload,sizeof(payload));func__Simple_DrainSerial();
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==1,"real UART early completion is retained once");
    clean();func__Sim_PutU32(&payload[4+6*4],0);func__Sim_SendFrame(SIMPLE_MSG_TLM_LIVE,payload,sizeof(payload));func__Simple_DrainSerial();
    func__Sim_PutU32(&payload[4+6*4],1);func__Sim_SendFrame(SIMPLE_MSG_TLM_LIVE,payload,sizeof(payload));func__Simple_DrainSerial();
    run(1,0,601);uint32_t uartStart=event(1).startSec,uartDuration=event(1).durationSec;
    UNSIGNED_LONG__G__MillisStart-=3600000u;func__Simple_ChargeTick();
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/diag",HTTP_GET);
    check(SIMPLE_WEB_SERVER_T__G__Server.simBody().find("\"status\":\"charging\"")!=std::string::npos,"HTTP history has no fabricated terminal outcome while UART is missing");
    func__Sim_PutU32(&payload[4+6*4],2);func__Sim_SendFrame(SIMPLE_MSG_TLM_LIVE,payload,sizeof(payload));func__Simple_DrainSerial();
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).startSec==uartStart&&event(1).durationSec==uartDuration,"real CRC-framed returning UART state resumes the same session without adding the missing hour");
    func__Sim_PutU32(&payload[4+6*4],3);func__Sim_SendFrame(SIMPLE_MSG_TLM_LIVE,payload,sizeof(payload));func__Simple_DrainSerial();
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/diag",HTTP_GET);
    check(UINT8_T__G__ChargeHistoryCount==1&&SIMPLE_WEB_SERVER_T__G__Server.simBody().find("\"status\":\"complete\"")!=std::string::npos,"UART completion after reconnection updates the same public HTTP history row");
    clean();uint8_t diag[39]={};diag[0]=0;func__Sim_PutU32(&diag[13],4);func__Sim_PutU32(&diag[23],10);
    func__Sim_SendFrame(SIMPLE_MSG_DIAG_DATA,diag,sizeof(diag));func__Simple_DrainSerial();func__Sim_PutU32(&diag[23],11);func__Sim_SendFrame(SIMPLE_MSG_DIAG_DATA,diag,sizeof(diag));func__Simple_DrainSerial();
    check(!UINT8_T__G__ChargeHistoryCount&&UINT32_T__G__ChargeCountCh1==11,"counter increases alone cannot fabricate dated completions");
    func__Simple_TimeSet(1793836800u,true);frame(0,0);frame(1,0);run(1,0,601);
    func__Sim_PutU32(&diag[13],5);func__Sim_SendFrame(SIMPLE_MSG_DIAG_DATA,diag,sizeof(diag));func__Simple_DrainSerial();
    check(event(1).status==0,"confirmed board reset immediately seals qualified history");
    clean();frame(0,0);frame(1,0);run(1,0,500);
    func__Sim_PutU32(&diag[13],6);func__Sim_SendFrame(SIMPLE_MSG_DIAG_DATA,diag,sizeof(diag));func__Simple_DrainSerial();
    frame(1,0);run(1,0,102);check(!UINT8_T__G__ChargeHistoryCount,"board reset discards a tentative candidate before it can qualify");
    clean();frame(99,0);frame(1,0);frame(3,0);check(!event(1).startSec,"invalid prior state cannot establish start time");
    clean();frame(5,0);frame(1,0);frame(3,0);check(!event(1).startSec,"initial retry wait cannot invent an earlier start");
    clean();frame(0,0);frame(4,0);frame(1,0);frame(3,0);check(event(1).startSec>=SIMPLE_TIME_VALID_MIN,"known inactive edge remains armed through bringup");
    // Cross the production observer's 32-bit millis wrap with a confirmed wall clock.
    UNSIGNED_LONG__G__MillisStart-=(uint32_t)(UINT32_MAX-300000u-(uint32_t)millis());
    clean();frame(0,0);frame(1,0);run(1,0,601);frame(3,0);
    check(UINT8_T__G__ChargeHistoryCount==1&&event(1).status==1&&event(1).durationSec<605u,"millis rollover neither splits a real session nor fabricates elapsed time");
    // The demo board must count completions, not minutes spent on the page.
    SIMPLE_WEB_SERVER_T__G__Server.on("/sim",HTTP_ANY,func__Sim_HttpSim);
    SIM_BOARD_STATE_T__G__Board = SimBoardState{};
    const uint32_t initialCount=SIM_BOARD_STATE_T__G__Board.chargeCount;
    for(uint32_t i=1;i<=180;i++) func__Sim_Tick(i*1000u);
    check(SIM_BOARD_STATE_T__G__Board.chargeCount==initialCount,"simulator does not invent a full charge every minute");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/sim",HTTP_POST,"s1=3");
    check(SIM_BOARD_STATE_T__G__Board.chargeCount==initialCount+1u,"simulator counts an actual upper-channel FLOAT transition");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/sim",HTTP_POST,"s1=3");
    check(SIM_BOARD_STATE_T__G__Board.chargeCount==initialCount+1u,"repeated simulated FLOAT never increments the completion count");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/sim",HTTP_POST,"s2=2");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/sim",HTTP_POST,"s2=10");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/sim",HTTP_POST,"s2=3");
    check(SIM_BOARD_STATE_T__G__Board.chargeCount==initialCount+2u&&SIM_BOARD_STATE_T__G__Board.chargeCountCh2==22u,"simulator retains lower-channel completion across thermal pause");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/sim",HTTP_POST,"s1=1");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/sim",HTTP_POST,"s1=7");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/sim",HTTP_POST,"s1=3");
    check(SIM_BOARD_STATE_T__G__Board.chargeCount==initialCount+2u,"simulated interrupted charge cannot become a later idle FLOAT completion");
    SIMPLE_WEB_SERVER_T__G__Server.simCall("/sim",HTTP_POST,"s2=2");
    uint32_t upperSec=SIM_BOARD_STATE_T__G__Board.chargeSecCh1,lowerSec=SIM_BOARD_STATE_T__G__Board.chargeSecCh2;
    func__Sim_Tick(181000u);
    check(SIM_BOARD_STATE_T__G__Board.chargeSecCh1==upperSec&&SIM_BOARD_STATE_T__G__Board.chargeSecCh2==lowerSec+1u,"simulator credits ABSORB time to the active lower channel, not the upper channel");
    std::filesystem::remove_all(root);printf("ALL %u REAL CHARGE SESSION CHECKS PASSED\n",checks);
}
