/* Reboot the actual 64 KiB LittleFS image after each physical program/erase boundary during a pack commit. */
#define main func__T_UnusedStorageStressMain
#include "host_test_storage_littlefs.cpp"
#undef main
#include <sys/mman.h>
#include <sys/wait.h>
struct CutSnapshot {unsigned operations;uint8_t image[65536];};
static CutSnapshot *snapshot;
static unsigned stopAt;
static void cutAfterOperation()
{
    snapshot->operations++;
    memcpy(snapshot->image,medium,sizeof(medium));
    if(snapshot->operations==stopAt)_exit(73);
}
static String readFile(const char *path){File f=LittleFS.open(path,"r");String text=f.readString();f.close();return text;}
static void seedFile(const char *path,const char *text){File f=LittleFS.open(path,"w");check(f&&f.print(text)==strlen(text),"seed sentinel");f.close();}
static void reboot(const uint8_t *image)
{
    check(streams.empty(),"no parent file handles");
    check(lfs_unmount(&flash)==0,"unmount parent");flash={};memcpy(medium,image,sizeof(medium));
    check(lfs_mount(&flash,&config)==0,"mount after simulated power cut");mounted=true;BOOL__G__FsOk=true;
    func__Simple_PackLoad();
}
int main()
{
    snapshot=(CutSnapshot*)mmap(nullptr,sizeof(CutSnapshot),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
    check(snapshot!=MAP_FAILED,"shared power-cut image");
    LittleFS.format();BOOL__G__FsOk=true;clockAt(1793836800u);
    func__Simple_CryptoKeyInit();func__Simple_AuthDefaults();check(func__Simple_AuthSave(),"seed authenticated settings");
    seedFile("/evt-20261105.csv","history sentinel");seedFile(SIMPLE_CHARGE_HISTORY_FILE,"charge history sentinel");
    SimplePackReview previous={500u,270u,330u,1793836800u,1u};check(func__Simple_PackSave(previous),"initial pack snapshot");
    const String committed=readFile(SIMPLE_PACK_REVIEW_FILE),users=readFile(SIMPLE_USERS_FILE);
    uint8_t baseline[65536];memcpy(baseline,medium,sizeof(medium));unsigned cuts=0,completed=0;
    for(unsigned scenario=0;scenario<2;scenario++){
        SimplePackReview next=previous;next.revision++;
        if(scenario==0)next.limit=900u;
        else {next.upper=280u;next.lower=340u;next.replacedAt+=100u;}
        const String replacement=func__Simple_PackEncode(next);
        bool finished=false;
        for(stopAt=1;stopAt<=128;stopAt++){
            reboot(baseline);snapshot->operations=0;memcpy(snapshot->image,baseline,sizeof(medium));
            pid_t pid=fork();check(pid>=0,"fork power-cut writer");
            if(pid==0){
                afterFlashOp=cutAfterOperation;
                bool saved=func__Simple_PackSave(next);
                afterFlashOp=nullptr;memcpy(snapshot->image,medium,sizeof(medium));_exit(saved?0:74);
            }
            int result;check(waitpid(pid,&result,0)==pid&&WIFEXITED(result),"writer exits at a defined flash boundary");
            int code=WEXITSTATUS(result);check(code==0||code==73,"writer succeeded or lost power, not a hidden storage error");
            reboot(snapshot->image);
            String recovered=readFile(SIMPLE_PACK_REVIEW_FILE);
            check(!BOOL__G__PackReviewInvalid&&(recovered==committed||recovered==replacement),"reboot recovers exactly old or new verified record, never a hybrid");
            check(readFile(SIMPLE_USERS_FILE)==users&&readFile("/evt-20261105.csv")=="history sentinel"&&readFile(SIMPLE_CHARGE_HISTORY_FILE)=="charge history sentinel","power cut never changes credentials or history");
            check(SIMPLE_PACK__G__Review.limit==(recovered==committed?previous.limit:next.limit),"reloaded RAM agrees with committed threshold");
            check(SIMPLE_PACK__G__Review.upper==(recovered==committed?previous.upper:next.upper)&&SIMPLE_PACK__G__Review.lower==(recovered==committed?previous.lower:next.lower),"reloaded baselines remain a matched pair");
            if(code==0){check(recovered==replacement,"uninterrupted commit persists requested record");finished=true;completed++;break;}
            cuts++;
        }
        check(finished,"enumerated every program/erase boundary through a successful save");
    }
    printf("REAL LITTLEFS PACK POWER-CUT PASS: scenarios=%u interrupted_commits=%u, physical_image=65536 bytes\n",completed,cuts);
    lfs_unmount(&flash);munmap(snapshot,sizeof(CutSnapshot));return 0;
}
