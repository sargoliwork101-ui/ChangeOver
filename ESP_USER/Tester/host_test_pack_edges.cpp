/* Regression boundaries for the production pack codec, auth, quota and freshness gates. */
#define main func__T_UnusedSimulatorMain
#include "../Simulator/main.cpp"
#undef main
#include <cassert>
#include <cstdio>
#include <filesystem>
static unsigned checks=0;
static void check(bool ok,const char *why){if(!ok){fprintf(stderr,"PACK EDGE FAIL: %s\n",why);abort();}checks++;}
static String disk(){File f=LittleFS.open(SIMPLE_PACK_REVIEW_FILE,"r");String s=f.readString();f.close();return s;}
static void writeFile(const char *path,const String &s){File f=LittleFS.open(path,"w");assert(f&&f.print(s)==s.length());f.close();}
static unsigned request(const char *path,int method,const String &args=""){SIMPLE_WEB_SERVER_T__G__Server.simCall(path,method,args.c_str());return SIMPLE_WEB_SERVER_T__G__Server.simStatus();}
static void fresh(){BOOL__G__TlmSeen=true;BOOL__G__DiagValid=true;BOOL__G__PackCountersSeen=true;UINT32_T__G__LastTlmMs=(uint32_t)millis();UINT32_T__G__PackCountersMs=(uint32_t)millis();UINT32_T__G__PackCounterReset=UINT32_T__G__ResetCount;}
static void codec()
{
    SimplePackReview record={300,270,330,1793836800u,2},out;
    String golden="PR1,300,270,330,1793836800,2,6506";
    check(func__Simple_PackEncode(record)==golden,"independent CRC16 golden vector");
    std::string raw=golden.c_str();
    for(size_t n=0;n<raw.size();n++)check(!func__Simple_PackDecode(String(raw.substr(0,n)),out),"every truncated record is rejected");
    for(size_t n=0;n<raw.size();n++)for(unsigned bit=0;bit<8;bit++){
        std::string changed=raw;changed[n]^=(char)(1u<<bit);
        check(!func__Simple_PackDecode(String(changed),out),"every single-bit corruption is rejected");
    }
    for(const char *bad:{"PR2,300,270,330,1793836800,2,6506","PR1,0300,270,330,1793836800,2,6506","PR1,300,270,330,1793836800,2,65536","PR1,300,270,330,1793836800,4294967296,0","PR1,1,0,0,0,0,0\n"})
        check(!func__Simple_PackDecode(bad,out),"noncanonical fields rejected");
    uint32_t seed=0x13579BDFu;
    for(unsigned i=0;i<256;i++){
        seed=seed*1664525u+1013904223u;
        SimplePackReview sample={seed%100001u,seed,seed^0xA5A5A5A5u,1793836800u,seed};
        check(func__Simple_PackDecode(func__Simple_PackEncode(sample),out)&&out.upper==sample.upper&&out.lower==sample.lower&&out.revision==sample.revision&&out.limit==sample.limit,"deterministic full-range codec roundtrip");
    }
    for(SimplePackReview bad: {SimplePackReview{100001,0,0,0,0},SimplePackReview{1,1,0,0,0},SimplePackReview{1,0,1,0,0},SimplePackReview{1,0,0,SIMPLE_TIME_VALID_MIN-1u,0}})
        check(!func__Simple_PackSave(bad),"invalid internal record never reaches disk");
}
static void statusAndStorage()
{
    SimplePackReview record={500,270,330,1793836800u,2};check(func__Simple_PackSave(record),"seed saved record");
    UINT32_T__G__ChargeCountCh1=270;UINT32_T__G__ChargeCountCh2=337;fresh();
    check(func__Simple_PackPublic().indexOf("\"count\":7")>=0,"lower-channel delta is independent");
    UINT32_T__G__ChargeCountCh1=669;UINT32_T__G__ChargeCountCh2=330;
    check(func__Simple_PackPublic().indexOf("\"state\":3")>=0,"one count below near boundary");
    UINT32_T__G__ChargeCountCh1=670;check(func__Simple_PackPublic().indexOf("\"state\":4")>=0,"exact near boundary");
    UINT32_T__G__ChargeCountCh1=769;check(func__Simple_PackPublic().indexOf("\"state\":4")>=0,"one count below due boundary");
    UINT32_T__G__ChargeCountCh1=770;check(func__Simple_PackPublic().indexOf("\"state\":5")>=0,"exact due boundary");
    UINT32_T__G__ChargeCountCh2=329;check(func__Simple_PackPublic().indexOf("\"valid\":0")>=0,"lower-channel rollback is neutral");
    for(bool *gate:{&BOOL__G__TlmSeen,&BOOL__G__DiagValid,&BOOL__G__PackCountersSeen}){fresh();*gate=false;check(!func__Simple_PackFresh(),"every freshness gate is required");}
    fresh();UINT32_T__G__PackCounterReset++;check(!func__Simple_PackFresh(),"different board boot cannot reuse counters");
    fresh();UINT32_T__G__PackCountersMs-=20001u;check(!func__Simple_PackFresh(),"counter expiry boundary");
    fresh();UINT32_T__G__LastTlmMs-=SIMPLE_LINK_TIMEOUT_MS+1u;check(!func__Simple_PackFresh(),"telemetry expiry boundary");
    fresh();BOOL__G__FsOk=false;check(func__Simple_PackPublic().indexOf("\"valid\":0")>=0,"filesystem loss neutralizes status");BOOL__G__FsOk=true;
    String before=disk();record.limit=700;
    SIM_BOOL__G__RefuseWrites=true;check(!func__Simple_PackSave(record)&&disk()==before&&SIMPLE_PACK__G__Review.limit==500,"refused write retains disk and RAM");SIM_BOOL__G__RefuseWrites=false;
    writeFile(SIMPLE_WIFI_FILE,String(std::string(SIMPLE_STORAGE_SETTINGS_BYTES,'x')));
    check(!func__Simple_PackSave(record)&&disk()==before,"settings quota includes pack record");LittleFS.remove(SIMPLE_WIFI_FILE);
    SIM_SIZE_T__G__FsTotalBytes=4096;check(!func__Simple_PackSave(record)&&disk()==before,"no physical room cannot report success");SIM_SIZE_T__G__FsTotalBytes=2097152;
    // A directory at the destination forces a real rename failure after successful staging verification.
    LittleFS.remove(SIMPLE_PACK_REVIEW_FILE);mkdir(func__Sim_FsPath(SIMPLE_PACK_REVIEW_FILE).c_str(),0700);
    check(!func__Simple_PackSave(record)&&SIMPLE_PACK__G__Review.limit==500&&!LittleFS.exists("/pack_review.txt.tmp"),"rename failure preserves RAM and removes staging");
    rmdir(func__Sim_FsPath(SIMPLE_PACK_REVIEW_FILE).c_str());writeFile(SIMPLE_PACK_REVIEW_FILE,before);
    writeFile(SIMPLE_PACK_REVIEW_FILE,String(std::string(128,'x')));func__Simple_PackLoad();check(BOOL__G__PackReviewInvalid,"oversized record is invalid");
    writeFile(SIMPLE_PACK_REVIEW_FILE,"");func__Simple_PackLoad();check(BOOL__G__PackReviewInvalid,"empty committed record is invalid");
    writeFile(SIMPLE_PACK_REVIEW_FILE,before);func__Simple_PackLoad();check(!BOOL__G__PackReviewInvalid,"valid disk recovery clears invalid state");
    record=SIMPLE_PACK__G__Review;record.limit=0;record.revision++;check(func__Simple_PackSave(record),"explicit reminder disable persists");
    fresh();UINT32_T__G__ChargeCountCh1=271;UINT32_T__G__ChargeCountCh2=330;
    check(func__Simple_PackPublic().indexOf("\"state\":2")>=0&&SIMPLE_PACK__G__Review.replacedAt==1793836800u,"disable preserves date and baseline");
}
static void authentication()
{
    func__Simple_AuthLogout();func__Simple_AuthDefaults();check(func__Simple_AuthLogin("admin","admin")==1,"administrator authenticated");
    std::string token=CHAR__G__SessionToken;SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token",token.c_str());
    String before=disk(),rev=String((unsigned long)SIMPLE_PACK__G__Review.revision);
    for(const char *bad:{"", "01", "+1", "-1", "1.0", "4294967296"})check(request("/pack/settings",HTTP_POST,String("limit=800&revision=")+bad)==409&&disk()==before,"noncanonical/missing revision never mutates disk");
    check(request("/pack/settings",HTTP_GET)==200&&SIMPLE_WEB_SERVER_T__G__Server.simHeader("Cache-Control")=="no-store","private response disables caching");
    check(request("/pack/replace",HTTP_GET,"confirm=REPLACE_PACK&revision=3")==404&&disk()==before,"GET cannot register replacement");
    check(request("/pack",HTTP_POST)==404&&disk()==before,"public status has no POST mutation route");
    check(request("/evt/file",HTTP_POST,"name=/pack_review.txt.tmp")==409,"staging record is protected from generic delete");
    UINT8_T__G__CurrentRole=0;check(request("/pack/settings",HTTP_GET)==403&&request("/pack/replace",HTTP_POST)==403,"viewer role cannot read private settings or replace");UINT8_T__G__CurrentRole=1;
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token","wrong");check(request("/pack/settings",HTTP_POST,String("limit=800&revision=")+rev)==403&&disk()==before,"wrong bearer cannot save");
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token","");
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("Cookie",(std::string(SIMPLE_SESSION_COOKIE)+"="+token).c_str());
    check(request("/pack/settings",HTTP_GET)==200,"owner cookie authenticates settings read");
    fresh();func__Simple_TimeSet(1793836700u,true);
    check(request("/pack/replace",HTTP_POST,String("confirm=REPLACE_PACK&revision=")+rev)==409&&disk()==before,"clock before prior replacement cannot register a new pack");
    UINT32_T__G__SessionStartMs=(uint32_t)millis()-SIMPLE_SESSION_TIMEOUT_MS-1u;
    check(request("/pack",HTTP_GET)==200,"public poll remains available after idle deadline");
    check(request("/pack/settings",HTTP_GET)==403&&request("/pack/settings",HTTP_POST,String("limit=900&revision=")+rev)==403&&disk()==before,"public polling cannot renew expired administrator access");
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("Cookie","");
    func__Simple_AuthLogout();check(func__Simple_AuthLogin("admin","admin")==1,"login after expiry");SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token",CHAR__G__SessionToken);
    SimplePackReview last=SIMPLE_PACK__G__Review;last.revision=UINT32_MAX;check(func__Simple_PackSave(last),"maximum revision persists");before=disk();
    check(request("/pack/settings",HTTP_POST,"limit=900&revision=4294967295")==409&&request("/pack/replace",HTTP_POST,"confirm=REPLACE_PACK&revision=4294967295")==409&&disk()==before,"revision exhaustion cannot wrap and replay");
}
int main()
{
    char root[]="/tmp/changeover-pack-edges-XXXXXX";check(mkdtemp(root)!=nullptr,"isolated test directory");
    setenv("SIMPLE_SIM_FS",root,1);setenv("SIMPLE_SIM_PORT","0",1);setup();
    codec();statusAndStorage();authentication();
    std::filesystem::remove_all(root);printf("ALL %u PACK EDGE CHECKS PASSED\n",checks);
}
