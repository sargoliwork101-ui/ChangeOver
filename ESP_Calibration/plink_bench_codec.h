/* [EN] Lossless bench envelope v1, CSV schema 3. No numeric conversions or heap allocations.
 * Each POST is one sequenced CRC32 frame. Only canonical settings lines are token-delta encoded.
 * Everything else (measurements, timestamps, signs, decimals, notes, repeats) is byte-for-byte raw.
 * A full settings checkpoint is emitted at least every 16 settings frames and at each run boundary.
 * Strict reading never skips damaged data or guesses a missing settings dependency. */
#ifndef PLINK_BENCH_CODEC_H
#define PLINK_BENCH_CODEC_H
struct EspBenchCodec {
    enum { Fields=171, HeaderSize=12, Prefix=12, MaskSize=(Fields+7)/8,
           MaxText=ESP_BENCHLOG_MAX_POST, FrameHead=11, FrameOverhead=15,
           MaxFrame=MaxText+FrameOverhead, Checkpoint=16 };
    struct State {
        char settings[MaxText+1] = {};
        uint32_t sequence=0u;
        uint32_t textBytes=0u;
        uint32_t offset=HeaderSize;
        uint8_t sinceFull=0u;
        bool valid=false;
        bool damaged=false;
        bool ioError=false;
    };
    static const uint8_t *header() {
        static const uint8_t h[HeaderSize]={'B','N','C','H',1,3,171,0,Checkpoint,0,0,0};
        return h;
    }
    static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|((uint16_t)p[1]<<8)); }
    static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
    static void put16(uint8_t *p,size_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
    static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i)); }
    static uint32_t crc(const uint8_t *p,size_t n) {
        uint32_t c=0xffffffffu;
        for(size_t i=0;i<n;++i) {
            c^=p[i];
            for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1u)?0xedb88320u:0u);
        }
        return ~c;
    }
    static bool text(const char *p,size_t n) {
        if(!p||!n||n>MaxText||p[n-1]!='\n')return false;
        for(size_t i=0;i<n;++i)if(p[i]!='\n'&&(p[i]<' '||p[i]>'~'))return false;
        return true;
    }
    /* [EN] Preserve lexical values exactly, including missing '-', -0 and leading zeros.
     * Unusual settings syntax is simply stored raw rather than interpreted or rounded. */
    static size_t token(const char *p) {
        size_t n=0u;
        if(*p=='-'){++p;++n;if(*p==','||*p=='\n')return n;}
        size_t digits=0u;
        while(*p>='0'&&*p<='9'&&n<11u){++p;++n;++digits;}
        return digits&&(*p==','||*p=='\n')?n:0u;
    }
    static bool settings(const char *p,size_t n) {
        if(!text(p,n)||n<=Prefix||memcmp(p,"# settings: ",Prefix)!=0)return false;
        size_t pos=Prefix;
        for(unsigned i=0;i<Fields;++i) {
            if(pos>=n)return false;
            const size_t k=token(p+pos);
            if(!k||pos+k>=n||p[pos+k]!=(i+1u==Fields?'\n':','))return false;
            pos+=k+1u;
        }
        return pos==n;
    }
    static void reset(State &s) {
        s.settings[0]='\0';s.sequence=0u;s.textBytes=0u;s.offset=HeaderSize;s.sinceFull=0u;
        s.valid=false;s.damaged=false;s.ioError=false;
    }
    /* [EN] 1=compact, 0=legacy text (download only), -1=damage, -2=I/O failure. */
    static int begin(File &f,State &s) {
        reset(s);
        if(!f.seek(0u)){s.ioError=true;return -2;}
        uint8_t h[HeaderSize]={};
        const size_t n=f.size()<HeaderSize?f.size():(size_t)HeaderSize;
        if(f.readBytes((char *)h,n)!=n){s.ioError=true;return -2;}
        if(n==HeaderSize&&memcmp(h,header(),HeaderSize)==0)return 1;
        if(n>=8u&&memcmp(h,"# cols:\n",8u)==0)return 0;
        s.damaged=true;return -1;
    }
    /* [EN] Pure encoder: the caller must not advance state until storage is confirmed. */
    static size_t encode(const State &s,const char *p,size_t n,uint8_t *out) {
        if(!text(p,n)||s.sequence==UINT32_MAX)return 0u;
        uint8_t type=0u;
        size_t payload=n;
        const bool isSettings=settings(p,n);
        if(isSettings) {
            type=1u;
            if(s.valid&&s.sinceFull+1u<Checkpoint) {
                memset(out+FrameHead,0,MaskSize);
                size_t old=Prefix,now=Prefix,used=MaskSize;
                for(unsigned i=0;i<Fields;++i) {
                    const size_t a=token(s.settings+old),b=token(p+now);
                    if(!a||!b)return 0u;
                    if(a!=b||memcmp(s.settings+old,p+now,b)!=0) {
                        if(used+1u+b>MaxText){used=MaxText+1u;break;}
                        out[FrameHead+i/8u]|=(uint8_t)(1u<<(i%8u));
                        out[FrameHead+used++]=(uint8_t)b;
                        memcpy(out+FrameHead+used,p+now,b);used+=b;
                    }
                    old+=a+1u;now+=b+1u;
                }
                if(used<n){type=2u;payload=used;}
            }
        }
        if(type!=2u)memcpy(out+FrameHead,p,n);
        out[0]='B';out[1]='F';put16(out+2,payload);
        put32(out+4,s.sequence+1u);out[8]=type;put16(out+9,n);
        const size_t total=FrameHead+payload;
        put32(out+total,crc(out,total));return total+4u;
    }
    static bool decode(State &s,const uint8_t *frame,size_t n,char *out) {
        if(n<FrameOverhead||n>MaxFrame||frame[0]!='B'||frame[1]!='F')return false;
        const size_t payload=u16(frame+2),count=u16(frame+9);
        const uint8_t type=frame[8];
        if(n!=payload+FrameOverhead||!count||count>MaxText||type>2u||
           s.sequence==UINT32_MAX||u32(frame+4)!=s.sequence+1u||
           crc(frame,n-4u)!=u32(frame+n-4u))return false;
        const uint8_t *in=frame+FrameHead;
        if(type==2u) {
            if(!s.valid||payload<MaskSize||(in[MaskSize-1]&0xf8u)||s.sinceFull+1u>=Checkpoint)return false;
            size_t cursor=MaskSize,old=Prefix,pos=Prefix;
            memcpy(out,"# settings: ",Prefix);
            for(unsigned i=0;i<Fields;++i) {
                const size_t a=token(s.settings+old);
                if(!a)return false;
                const char *value=s.settings+old;size_t k=a;
                if(in[i/8u]&(1u<<(i%8u))) {
                    if(cursor>=payload)return false;
                    k=in[cursor++];
                    if(!k||k>11u||cursor+k>payload)return false;
                    value=(const char *)in+cursor;cursor+=k;
                }
                if(pos+k+1u>count)return false;
                memcpy(out+pos,value,k);pos+=k;out[pos++]=i+1u==Fields?'\n':',';old+=a+1u;
            }
            if(cursor!=payload||pos!=count)return false;
        } else {
            if(payload!=count)return false;
            memcpy(out,in,count);
        }
        out[count]='\0';
        if(!text(out,count)||(type&& !settings(out,count))||s.textBytes>UINT32_MAX-count)return false;
        if(type) {
            memcpy(s.settings,out,count+1u);s.valid=true;
            s.sinceFull=type==1u?0u:(uint8_t)(s.sinceFull+1u);
        } else if(count>=6u&&memcmp(out,"# run ",6u)==0) {s.valid=false;s.sinceFull=0u;}
        ++s.sequence;s.textBytes+=(uint32_t)count;return true;
    }
    /* [EN] Strict record scan; no resynchronization and no silent tail repair. */
    static bool next(File &f,State &s,uint8_t *frame,char *out) {
        const size_t size=f.size();
        if(s.offset==size)return false;
        if(s.offset>size||size-s.offset<FrameOverhead){s.damaged=true;return false;}
        if(f.readBytes((char *)frame,FrameHead)!=FrameHead){s.ioError=true;return false;}
        const size_t payload=u16(frame+2);
        if(payload>MaxText||size-s.offset<payload+FrameOverhead){s.damaged=true;return false;}
        if(f.readBytes((char *)frame+FrameHead,payload+4u)!=payload+4u){s.ioError=true;return false;}
        if(!decode(s,frame,payload+FrameOverhead,out)){s.damaged=true;return false;}
        s.offset+=(uint32_t)(payload+FrameOverhead);return true;
    }
};
#endif
