/* Production storage paths on an actual 64 KiB LittleFS image, not host-file sizes.
   Build with run_storage_littlefs.sh and LITTLEFS_SOURCE pointing at upstream sources. */
#include "stub_arduino.h"
#include "stub_web_server.h"
extern "C" {
#include "lfs.h"
}
#include <cassert>
#include <map>
#include <algorithm>
typedef ESP8266WebServer esp_web_server_t;
#include "../simple_config.h"
#include "../simple_state.h"
#include "../simple_panel.h"
#include "../simple_link.h"
#include "../simple_auth.h"
#include "../simple_theme.h"
#include "../simple_diag.h"
#include "../simple_http.h"
SimSerialLink Serial;SimWiFi WiFi;SimEsp ESP;SimLittleFS LittleFS;
static lfs_t flash;
static uint8_t medium[65536];
static bool mounted=false;
static int failures=0,peak=0;
static int rd(const lfs_config*c,lfs_block_t b,lfs_off_t o,void*p,lfs_size_t n){memcpy(p,medium+b*c->block_size+o,n);return 0;}
static int wr(const lfs_config*c,lfs_block_t b,lfs_off_t o,const void*p,lfs_size_t n){
    auto src=(const uint8_t*)p;for(unsigned i=0;i<n;i++){auto &d=medium[b*c->block_size+o+i];assert((d&src[i])==src[i]);d&=src[i];}return 0;
}
static int erase(const lfs_config*c,lfs_block_t b){memset(medium+b*c->block_size,255,c->block_size);return 0;}
static int sync(const lfs_config*){return 0;}
static lfs_config config;
struct Cookie {lfs_file_t file;FILE* stream;int descriptor;};
static std::map<FILE*,Cookie*> streams;
static std::map<int,Cookie*> descriptors;
static int nextDescriptor=10000;
static ssize_t cookieRead(void *p,char *out,size_t n){return lfs_file_read(&flash,&((Cookie*)p)->file,out,n);}
static ssize_t cookieWrite(void *p,const char *in,size_t n){int r=lfs_file_write(&flash,&((Cookie*)p)->file,in,n);if(r<0)failures++;return r<0?-1:r;}
static int cookieSeek(void*p,off64_t*o,int whence){int r=lfs_file_seek(&flash,&((Cookie*)p)->file,*o,whence);if(r<0)return -1;*o=r;return 0;}
static int cookieClose(void*p){auto c=(Cookie*)p;int r=lfs_file_close(&flash,&c->file);peak=std::max(peak,(int)lfs_fs_size(&flash)*4096);streams.erase(c->stream);descriptors.erase(c->descriptor);delete c;return r;}
extern "C" FILE *__real_fopen(const char*,const char*);
extern "C" int __real_fileno(FILE*);
extern "C" int __real_ftruncate(int,off_t);
extern "C" FILE *__wrap_fopen(const char *path,const char *mode){
    if(strncmp(path,"/lfs/",5))return __real_fopen(path,mode);
    auto c=new Cookie{};int flags=mode[0]=='r'?LFS_O_RDONLY:LFS_O_WRONLY|LFS_O_CREAT;
    if(strchr(mode,'+'))flags=LFS_O_RDWR;
    if(mode[0]=='w')flags|=LFS_O_TRUNC;
    if(mode[0]=='a')flags|=LFS_O_APPEND;
    if(lfs_file_open(&flash,&c->file,path+4,flags)){delete c;return nullptr;}
    cookie_io_functions_t ops={cookieRead,cookieWrite,cookieSeek,cookieClose};
    c->stream=fopencookie(c,mode,ops);assert(c->stream);setvbuf(c->stream,nullptr,_IONBF,0);
    c->descriptor=nextDescriptor++;streams[c->stream]=c;descriptors[c->descriptor]=c;return c->stream;
}
extern "C" int __wrap_fileno(FILE *f){auto it=streams.find(f);return it==streams.end()?__real_fileno(f):it->second->descriptor;}
extern "C" int __wrap_ftruncate(int fd,off_t n){auto it=descriptors.find(fd);return it==descriptors.end()?__real_ftruncate(fd,n):lfs_file_truncate(&flash,&it->second->file,n);}
bool SimLittleFS::begin(bool){return mounted;}
bool SimLittleFS::format(){
    assert(streams.empty());if(mounted)assert(lfs_unmount(&flash)==0);memset(medium,255,sizeof(medium));
    config={};config.read=rd;config.prog=wr;config.erase=erase;config.sync=sync;config.read_size=64;config.prog_size=64;
    config.block_size=4096;config.block_count=16;config.cache_size=64;config.lookahead_size=64;config.block_cycles=16;
    assert(lfs_format(&flash,&config)==0);assert(lfs_mount(&flash,&config)==0);mounted=true;return true;
}
bool SimLittleFS::info(FSInfo &i){i.totalBytes=sizeof(medium);i.blockSize=4096;i.usedBytes=lfs_fs_size(&flash)*4096;peak=std::max(peak,(int)i.usedBytes);return true;}
File SimLittleFS::open(const char*p,const char*m){return File(std::string("/lfs")+p,m);}
bool SimLittleFS::exists(const char*p){lfs_info i;return lfs_stat(&flash,p,&i)==0;}
bool SimLittleFS::remove(const char*p){return lfs_remove(&flash,p)==0;}
bool SimLittleFS::rename(const char*a,const char*b){return lfs_rename(&flash,a,b)==0;}
SimDir SimLittleFS::openDir(const char*p){SimDir result;lfs_dir_t dir;assert(lfs_dir_open(&flash,&dir,p)==0);lfs_info i;while(lfs_dir_read(&flash,&dir,&i)>0)if(i.type==LFS_TYPE_REG){result.names.push_back(i.name);result.sizes.push_back(i.size);}lfs_dir_close(&flash,&dir);return result;}
static void check(bool ok,const char*why){if(!ok){fprintf(stderr,"LITTLEFS FAIL: %s, used=%d, free=%u\n",why,(int)lfs_fs_size(&flash)*4096,func__SimpleLog_FreeBytes());abort();}}
static void clockAt(uint32_t epoch){func__Simple_TimeSet(epoch,true);BOOL__G__TlmSeen=true;UINT32_T__G__LastTlmMs=(uint32_t)millis();}
int main(int argc,char**argv){
    int profile=argc>1?atoi(argv[1]):1;
    LittleFS.format();BOOL__G__FsOk=true;func__Simple_CryptoKeyInit();func__Simple_AuthDefaults();
    check(func__Simple_AuthSave(),"initial account");check(func__Simple_WifiSave(),"initial wifi");
    func__Simple_ParamsSave();clockAt(1793836800u);func__Simple_TimeSave();
    const std::string users=[](){File f=LittleFS.open(SIMPLE_USERS_FILE,"r");return std::string(f.readString().c_str());}();
    unsigned blocked=0,observations=0;
    unsigned long realStart;{timeval tv;gettimeofday(&tv,nullptr);realStart=(unsigned long)tv.tv_sec*1000+tv.tv_usec/1000;}
    for(unsigned minute=0;minute<90u*1440u;minute++){
        UNSIGNED_LONG__G__MillisStart=realStart-(unsigned long)minute*60000u;
        clockAt(1793836800u+minute*60u);unsigned cycle=profile==2?minute%720:minute%1440;
        bool cut=profile>0&&cycle>=120&&cycle<240;uint32_t pack=27000;
        if(profile>0){if(cut)pack=27000-(cycle-120)*30;else if(cycle>=240&&cycle<480)pack=23400+(cycle-240)*15;}
        pack+=(minute%7)*4;UINT8_T__G__TlmFlags=cut?0:2;
        UINT32_T__G__TlmField[14]=cut?0:28000+(minute%5)*10;UINT32_T__G__TlmField[15]=pack;
        UINT32_T__G__TlmField[16]=pack/2;UINT32_T__G__TlmField[17]=pack/2-10;UINT32_T__G__TlmField[18]=pack/2+10;
        UINT32_T__G__TlmField[25]=20;
        unsigned state=cut?0:(cycle>=240&&cycle<360?1:(cycle>=360&&cycle<480?2:3));
        UINT32_T__G__TlmField[6]=state;UINT32_T__G__TlmField[13]=state;
        UINT32_T__G__TlmField[3]=state==1?1200:state==2?600:50;UINT32_T__G__TlmField[10]=UINT32_T__G__TlmField[3];
        UINT32_T__G__TlmField[5]=state==1?800:100;UINT32_T__G__TlmField[12]=UINT32_T__G__TlmField[5];
        func__SimpleLog_OnFrame();func__SimpleLog_Tick();func__Simple_ChartTick();observations++;
        if(BOOL__G__ChartPending)blocked++;
        if(BOOL__G__DiagDirty)func__Simple_DiagSave();
        if(profile>0&&cycle==480){
            for(unsigned i=SIMPLE_DIAG_MAX_POWER_EVENTS-1;i>0;i--)SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[i]=SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[i-1];
            SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[0]={func__Simple_NowSec(),1u,14400u};
            if(UINT8_T__G__ChargeHistoryCount<SIMPLE_DIAG_MAX_POWER_EVENTS)UINT8_T__G__ChargeHistoryCount++;
            BOOL__G__ChargeHistDirty=true;func__Simple_ChargeHistorySave();
        }
        check(!BOOL__G__DiagDirty&&!BOOL__G__ChargeHistDirty,"history save progressed");
        check(UINT8_T__G__LogRamCount==0&&UINT16_T__G__LogDroppedRows==0,"new logs continue");
        check(!BOOL__G__ChartPending,"chart save progressed");
        FSInfo i;LittleFS.info(i);check(i.usedBytes<=sizeof(medium),"physical cap");
        char path[SIMPLE_LOG_PATH_MAX];func__SimpleLog_PathOf(path,UINT32_T__G__LogDayCode);
        check(func__Simple_StorageFileBytes(path)<=SIMPLE_LOG_DAY_MAX_BYTES,"day byte bound");
    }
    for (unsigned burst=0;burst<1000u;burst++) {
        clockAt(1793836800u+90u*86400u-60u);
        UINT32_T__G__TlmField[15]=22000u+(burst%30u)*200u;
        check(func__SimpleLog_WriteRow(SIMPLE_LOG_CHG_VSTEP,0u),"within-day burst persists");
        FSInfo i;LittleFS.info(i);
    }
    char dayPath[SIMPLE_LOG_PATH_MAX];func__SimpleLog_PathOf(dayPath,UINT32_T__G__LogDayCode);
    check(func__Simple_StorageFileBytes(dayPath)<=SIMPLE_LOG_DAY_MAX_BYTES,"burst compacted to one block");
    check(func__Simple_ChartCheckpoint(),"final chart compaction");
    unsigned count=UINT16_T__G__Chart3M_Count;func__Simple_ChartsLoad();check(UINT16_T__G__Chart3M_Count==count,"chart replay");
    check(func__Simple_ChartsClear(),"durable chart clear");func__Simple_ChartsLoad();check(UINT16_T__G__Chart3M_Count==0,"cleared chart reboot");
    check(func__Simple_DiagClear(),"durable diagnostic clear");check(func__Simple_ChargeHistoryClear(),"durable charge clear");
    File f=LittleFS.open(SIMPLE_USERS_FILE,"r");check(std::string(f.readString().c_str())==users,"account bytes unchanged");f.close();
    check(failures==0,"no physical ENOSPC writes");func__SimpleLog_ScanDays(true);
    printf("REAL LITTLEFS 64KiB PASS: profile=%d minutes=%u retained_days=%u reclaimed=%u peak_allocated=%d blocked=%u write_errors=%d\n",profile,observations,UINT8_T__G__LogIndexCount,UINT16_T__G__LogReclaimed,peak,blocked,failures);
    check(streams.empty(),"all files closed");lfs_unmount(&flash);return 0;
}
