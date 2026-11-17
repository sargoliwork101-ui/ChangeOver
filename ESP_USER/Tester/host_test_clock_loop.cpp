/* Run the real sketch setup/loop without a responding simulated board. */
#define main func__T_UnusedSimulatorMain
#include "../Simulator/main.cpp"
#undef main
#include <cassert>
#include <cstdio>
static unsigned func__T_CountOutgoing(uint8_t wanted)
{
    std::vector<uint8_t> bytes;uint8_t byte;
    while(Serial.takeEspByte(&byte))bytes.push_back(byte);
    unsigned count=0u;
    for(size_t pos=0u;pos+8u<=bytes.size();)
    {
        assert(bytes[pos]==SIMPLE_LINK_SOF_BYTE0 && bytes[pos+1u]==SIMPLE_LINK_SOF_BYTE1);
        size_t length=(size_t)bytes[pos+4u]+((size_t)bytes[pos+5u]<<8u);
        assert(pos+8u+length<=bytes.size());
        if(bytes[pos+3u]==wanted)count++;
        assert(bytes[pos+3u]!=SIMPLE_MSG_TIME_SYNC);
        pos+=length+8u;
    }
    return count;
}
int main()
{
    setenv("SIMPLE_SIM_FS","/tmp/changeover_clock_loop_test_fs",1);
    setenv("SIMPLE_SIM_PORT","0",1);
    BOOL__G__FsOk=LittleFS.begin();assert(BOOL__G__FsOk);assert(LittleFS.format());
    const uint32_t base=1793836790u;
    func__Simple_TimeSet(base,true);func__Simple_TimeNoteBoard(100u,true,3u);func__Simple_TimeSave();
    setup();
    assert(!func__Simple_TimeBootReady());
    unsigned requests=0u;
    for(unsigned seconds=1u;seconds<=40u;seconds++)
    {
        UNSIGNED_LONG__G__MillisStart-=1000u;
        BOOL__G__TlmSeen=true;UINT32_T__G__LastTlmMs=(uint32_t)millis();
        BOOL__G__DiagValid=true; // Stored diagnostics are not a fresh clock confirmation.
        loop();requests+=func__T_CountOutgoing(SIMPLE_MSG_DIAG_REQ);
        assert(!func__Simple_TimeBootReady());
        assert(UINT16_T__G__Chart1D_Count==0u && UINT16_T__G__LogRowsToday==0u);
        if(seconds<40u)assert(!LittleFS.exists(SIMPLE_TIME_GAP_FILE));
    }
    assert(requests>=3u && LittleFS.exists(SIMPLE_TIME_GAP_FILE));
    uint8_t payload[23]={};const uint32_t uptime=260u;
    for(unsigned i=0u;i<4u;i++)payload[9u+i]=(uint8_t)(uptime>>(i*8u));
    payload[13]=3u;
    func__Sim_SendFrame(SIMPLE_MSG_DIAG_DATA,payload,sizeof(payload));
    loop();
    assert(func__Simple_TimeBootReady() && func__Simple_NowSec()==base+160u);
    UNSIGNED_LONG__G__MillisStart-=5000u;loop();
    assert(!LittleFS.exists(SIMPLE_TIME_GAP_FILE));
    char path[SIMPLE_LOG_PATH_MAX];func__SimpleLog_PathOf(path,func__SimpleLog_TodayCode());
    File file=LittleFS.open(path,"r");SimpleLogCodec::State state;unsigned warnings=0u;
    assert(file && SimpleLogCodec::begin(file,state)==1);
    while(SimpleLogCodec::next(file,state))if(state.value[3]==SIMPLE_LOG_NOTE_CLOCK_GAP)warnings++;
    assert(!state.ioError && warnings==1u);
    printf("REAL SETUP/LOOP CLOCK PASS: %u repeated DIAG requests, no stale TIME_SYNC, 40-second durable warning, late UART recovery without browser\n",requests);
    return 0;
}
