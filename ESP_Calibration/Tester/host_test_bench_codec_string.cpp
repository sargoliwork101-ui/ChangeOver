/* [EN] Regression for ESP8266-style free String + enum overload candidates. */
#include "stub_arduino.h"
#include <type_traits>
#include <utility>
#include <cassert>
/* [EN] The normal host String stub lacks these core-style overloads.
 * Restrict them to enums to isolate this warning without changing the shared stub.
 * Only the implicit character-pointer constructor participates here, avoiding
 * the host stub's extra implicit numeric conversions. */
class CoreStringCandidate {
public:
    CoreStringCandidate(const char *) {}
};
template<class T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
CoreStringCandidate operator+(const CoreStringCandidate &, const T &) { std::abort(); }
template<class T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
CoreStringCandidate operator+(CoreStringCandidate &&, const T &) { std::abort(); }
#define ESP_BENCHLOG_MAX_POST 1536u
#ifndef BENCH_CODEC_HEADER
#define BENCH_CODEC_HEADER "../plink_bench_codec.h"
#endif
#include BENCH_CODEC_HEADER
int main()
{
    StubFS fs;
    uint8_t encoded[EspBenchCodec::MaxFrame];
    uint8_t guarded[EspBenchCodec::MaxFrame+2];
    char text[EspBenchCodec::MaxText+1];
    for(size_t count=1u;count<=EspBenchCodec::MaxText;++count) {
        std::string original(count,'~');original.back()='\n';
        EspBenchCodec::State encoder,reader;
        const size_t length=EspBenchCodec::encode(encoder,original.data(),original.size(),encoded);
        assert(length==count+EspBenchCodec::FrameOverhead);
        fs.files["/bench"]=std::string((const char *)EspBenchCodec::header(),EspBenchCodec::HeaderSize)+
                            std::string((const char *)encoded,length);
        File file=fs.open("/bench","r");
        assert(EspBenchCodec::begin(file,reader)==1);
        memset(guarded,0xa5,sizeof guarded);
        assert(EspBenchCodec::next(file,reader,guarded+1,text));
        assert(std::string(text)==original);
        assert(memcmp(guarded+1,encoded,length)==0);
        assert(guarded[0]==0xa5&&guarded[sizeof guarded-1]==0xa5);
        assert(!reader.ioError&&!reader.damaged&&reader.offset==file.size());
        assert(!EspBenchCodec::next(file,reader,guarded+1,text));
    }
    File shortRead=fs.open("/bench","r");
    EspBenchCodec::State reader;
    assert(EspBenchCodec::begin(shortRead,reader)==1);
    fs.readLimit=EspBenchCodec::FrameHead;
    assert(!EspBenchCodec::next(shortRead,reader,guarded+1,text));
    assert(reader.ioError&&!reader.damaged&&reader.sequence==0u);
    std::puts("PASS: String-overload regression; all 1536 payload lengths round-trip exactly; canaries and short-read handling preserved.");
}
