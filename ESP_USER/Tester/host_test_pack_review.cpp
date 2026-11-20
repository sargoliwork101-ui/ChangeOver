/* Test real ESP maintenance routes, committed storage, UART freshness and replacement semantics. */
#define main func__T_UnusedSimulatorMain
#include "../Simulator/main.cpp"
#undef main
#include <cassert>
#include <cstdio>
static unsigned checks=0u;
static void check(bool ok,const char *message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);abort();}checks++;printf("PASS: %s\n",message);}
static void call(const char *url, int method,const String &body="")
{SIMPLE_WEB_SERVER_T__G__Server.simCall(url,method,body.c_str());}
static unsigned status(){return SIMPLE_WEB_SERVER_T__G__Server.simStatus();}
static String body(){return SIMPLE_WEB_SERVER_T__G__Server.simBody();}
static String bytes(const char *path){File f=LittleFS.open(path,"r");String out=f.readString();f.close();return out;}
static String revision(){return String((unsigned long)SIMPLE_PACK__G__Review.revision);}
static void diag(unsigned length,uint32_t up=270u,uint32_t down=330u)
{
    memset(UINT8_T__G__RxPayload,0,sizeof(UINT8_T__G__RxPayload));
    uint32_t values[]={0u,0u,100u,3u,1793836800u};
    for(unsigned n=0;n<5;n++)for(unsigned i=0;i<4;i++)UINT8_T__G__RxPayload[1u+n*4u+i]=(uint8_t)(values[n]>>(8u*i));
    for(unsigned i=0;i<4;i++){UINT8_T__G__RxPayload[23u+i]=(uint8_t)(up>>(8u*i));UINT8_T__G__RxPayload[27u+i]=(uint8_t)(down>>(8u*i));}
    UINT8_T__G__RxType=SIMPLE_MSG_DIAG_DATA;UINT16_T__G__RxLen=length;
    func__Simple_HandleFrame();
    BOOL__G__TlmSeen=true;UINT32_T__G__LastTlmMs=(uint32_t)millis();
}
static void replace(){call("/pack/replace",HTTP_POST,String("confirm=REPLACE_PACK&revision=")+revision());}
int main()
{
    setenv("SIMPLE_SIM_FS","/tmp/changeover_pack_review_fs",1);setenv("SIMPLE_SIM_PORT","0",1);
    BOOL__G__FsOk=LittleFS.begin();assert(LittleFS.format());setup();
    call("/pack.js",HTTP_GET);check(status()==200&&body()==SIMPLE_PACK_REVIEW_JS,"real flash route serves exact production UI");
    call("/pack",HTTP_GET);check(body().indexOf("\"valid\":0")>=0&&body().indexOf("limit")<0,"public status is neutral before fresh counters and omits settings");
    call("/pack/settings",HTTP_GET);check(status()==403,"settings read requires server authentication");
    call("/pack/settings",HTTP_POST,"limit=300&revision=0");check(status()==403,"threshold mutation requires authentication");replace();check(status()==403,"replacement mutation requires authentication");
    func__Simple_AuthDefaults();check(func__Simple_AuthLogin("admin","admin")==1,"real administrator login succeeds");
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token",CHAR__G__SessionToken);
    call("/pack/settings",HTTP_POST,"limit=300&revision=0");check(status()==200&&SIMPLE_PACK__G__Review.limit==300u,"threshold persists on ESP");
    String old=bytes(SIMPLE_PACK_REVIEW_FILE);func__Simple_PackLoad();check(SIMPLE_PACK__G__Review.limit==300u&&!BOOL__G__PackReviewInvalid,"committed threshold survives reload");
    for(const char *bad:{"-1","1.5","100001","300x","01","4294967296",""}){
        call("/pack/settings",HTTP_POST,String("limit=")+bad+"&revision="+revision());check(status()==400&&bytes(SIMPLE_PACK_REVIEW_FILE)==old,"invalid limit cannot replace saved record");
    }
    call("/pack/settings",HTTP_POST,"limit=900&revision=0");check(status()==409,"stale revision cannot mutate the record");
    call("/pack/replace",HTTP_POST,String("revision=")+revision());check(status()==400,"explicit replacement confirmation is required");
    BOOL__G__DiagValid=true;UINT32_T__G__ChargeCountCh1=270;UINT32_T__G__ChargeCountCh2=330;
    replace();check(status()==409,"restored lifetime counters are not fresh data");
    diag(23);replace();check(status()==409&&!BOOL__G__PackCountersSeen,"short diagnostic packets cannot authorize replacement");
    diag(39);BOOL__G__TimeValid=false;replace();check(status()==409,"replacement is blocked without trusted date");
    func__Simple_TimeSet(1793836800u,true);BOOL__G__TimeFromDisk=true;BOOL__G__TimeBoardSeen=false;replace();check(status()==409,"saved clock alone is not a current date");
    BOOL__G__TimeFromDisk=false;diag(39);
    call("/pack",HTTP_GET);check(body().indexOf("\"count\":330")>=0&&body().indexOf("\"state\":5")>=0,"pack uses maximum per-channel count before replacement");
    const char *historyPath="/evt-20261105.csv";File history=LittleFS.open(historyPath,"w");history.print("sentinel history");history.close();
    File charge=LittleFS.open(SIMPLE_CHARGE_HISTORY_FILE,"w");charge.print("sentinel charge");charge.close();
    String priorRevision=revision();replace();check(status()==200&&SIMPLE_PACK__G__Review.upper==270&&SIMPLE_PACK__G__Review.lower==330&&SIMPLE_PACK__G__Review.replacedAt>=1793836800u,"replacement captures both current baselines and date");
    check(SIMPLE_PACK__G__Review.limit==300&&UINT32_T__G__ChargeCountCh1==270&&UINT32_T__G__ChargeCountCh2==330&&bytes(historyPath)=="sentinel history"&&bytes(SIMPLE_CHARGE_HISTORY_FILE)=="sentinel charge","replacement preserves threshold, lifetime counters and history");
    uint8_t byte;check(!Serial.takeEspByte(&byte),"maintenance sends no STM32 commands");
    call("/pack",HTTP_GET);check(body().indexOf("\"count\":0")>=0&&body().indexOf("\"state\":3")>=0,"confirmed replacement begins at zero");
    call("/pack/replace",HTTP_POST,String("confirm=REPLACE_PACK&revision=")+priorRevision);check(status()==409,"duplicate replacement request cannot rebase a second time");
    diag(39,271,330);call("/pack",HTTP_GET);check(body().indexOf("\"count\":1")>=0,"independent upper-channel delta is one even below prior absolute maximum");
    uint32_t savedAt=SIMPLE_PACK__G__Review.replacedAt;
    call("/pack/settings",HTTP_POST,String("limit=500&revision=")+revision());check(status()==200&&SIMPLE_PACK__G__Review.upper==270&&SIMPLE_PACK__G__Review.replacedAt==savedAt,"threshold change preserves replacement baseline and date");
    func__Simple_PackLoad();call("/pack",HTTP_GET);check(body().indexOf("\"count\":1")>=0&&SIMPLE_PACK__G__Review.replacedAt==savedAt,"replacement survives ESP record reload");
    old=bytes(SIMPLE_PACK_REVIEW_FILE);SIM_BOOL__G__ShortPrint=true;
    call("/pack/settings",HTTP_POST,String("limit=700&revision=")+revision());check(status()==503&&bytes(SIMPLE_PACK_REVIEW_FILE)==old&&SIMPLE_PACK__G__Review.limit==500,"short threshold write preserves old disk and RAM");
    replace();check(status()==503&&bytes(SIMPLE_PACK_REVIEW_FILE)==old&&SIMPLE_PACK__G__Review.upper==270,"short replacement write preserves prior baseline and date");
    call("/pack/settings",HTTP_POST,String("limit=500&revision=")+revision());check(status()==200&&bytes(SIMPLE_PACK_REVIEW_FILE)==old,"unchanged setting verifies disk without another flash write");
    SIM_BOOL__G__ShortPrint=false;
    File staged=LittleFS.open("/pack_review.txt.tmp","w");staged.print("torn staging");staged.close();
    func__Simple_PackLoad();check(bytes(SIMPLE_PACK_REVIEW_FILE)==old&&!BOOL__G__PackReviewInvalid,"orphan staging never replaces the committed record at boot");
    LittleFS.remove("/pack_review.txt.tmp");
    mkdir(func__Sim_FsPath("/pack_review.txt.tmp").c_str(),0700);
    File blocker=LittleFS.open("/pack_review.txt.tmp/blocker","w");blocker.print("x");blocker.close();
    call("/pack/settings",HTTP_POST,String("limit=800&revision=")+revision());
    check(status()==503&&bytes(SIMPLE_PACK_REVIEW_FILE)==old,"unremovable staging failure preserves committed configuration");
    LittleFS.remove("/pack_review.txt.tmp/blocker");rmdir(func__Sim_FsPath("/pack_review.txt.tmp").c_str());
    SimplePackReview maxRecord={100000u,UINT32_MAX,UINT32_MAX,1793836800u,UINT32_MAX};
    SimplePackReview maxDecoded;check(func__Simple_PackDecode(func__Simple_PackEncode(maxRecord),maxDecoded)&&maxDecoded.upper==UINT32_MAX,"record codec supports full uint32 counter range");
    BOOL__G__FsOk=false;replace();check(status()==503&&bytes(SIMPLE_PACK_REVIEW_FILE)==old,"unavailable filesystem cannot report successful replacement");BOOL__G__FsOk=true;
    diag(39,269,330);call("/pack",HTTP_GET);check(body().indexOf("\"valid\":0")>=0&&body().indexOf("\"state\":1")>=0,"counter rollback is unknown, never false zero");
    diag(39,670,330);call("/pack",HTTP_GET);check(body().indexOf("\"state\":4")>=0,"eighty percent enters near-review state");
    diag(39,770,330);call("/pack",HTTP_GET);check(body().indexOf("\"state\":5")>=0,"configured threshold enters review-due state");
    UINT32_T__G__PackCountersMs-=20001u;replace();check(status()==409,"stale per-channel diagnostics cannot register replacement");
    call("/pack",HTTP_GET);check(body().indexOf("\"state\":0")>=0,"stale counters neutralize public status");
    diag(39);UINT32_T__G__LastTlmMs-=SIMPLE_LINK_TIMEOUT_MS+1u;replace();check(status()==409,"disconnected telemetry cannot register replacement");
    diag(39);call("/evt/file",HTTP_POST,"name=/pack_review.txt");check(status()==409&&bytes(SIMPLE_PACK_REVIEW_FILE)==old,"generic deletion cannot erase replacement tracking");
    SimplePackReview decoded;check(!func__Simple_PackDecode(old+"x",decoded)&&!func__Simple_PackDecode("PR1,4294967296,0,0,0,0,1",decoded),"strict decoder rejects trailing garbage and overflow");
    File corrupt=LittleFS.open(SIMPLE_PACK_REVIEW_FILE,"w");corrupt.print("PR1,broken");corrupt.close();func__Simple_PackLoad();call("/pack",HTTP_GET);check(BOOL__G__PackReviewInvalid&&body().indexOf("\"state\":1")>=0,"corrupt committed record stays neutral");
    call("/pack/settings",HTTP_POST,"limit=300&revision=0");check(status()==409,"threshold edit cannot silently erase a corrupt replacement baseline");
    check(func__Simple_StorageWrite(SIMPLE_PACK_REVIEW_FILE,old),"restore committed record for format test");func__Simple_PackLoad();
    call("/evt/format",HTTP_POST);check(status()==200&&bytes(SIMPLE_PACK_REVIEW_FILE)==old,"ordinary format preserves maintenance configuration");
    BOOL__G__PackCountersSeen=false;func__Simple_PackLoad();call("/pack",HTTP_GET);check(body().indexOf("\"valid\":0")>=0,"reboot never treats restored maintenance record as fresh counters");
    call("/factory/reset",HTTP_POST,"confirm=ERASE_ESP");check(status()==200&&!LittleFS.exists(SIMPLE_PACK_REVIEW_FILE),"confirmed ESP factory reset erases maintenance configuration");
    printf("ALL %u PRODUCTION PACK CHECKS PASSED\n",checks);
}
