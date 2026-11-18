/* [EN] Production codec + public HTTP handlers, with partial-write/read/restart fault injection. */
static void test_bench_codec()
{
    const char *path=ESP_BENCHLOG_PATH;
    const auto reset=[&]() {
        LittleFS.files.clear();LittleFS.failOpenCount=0;LittleFS.failWrites=false;LittleFS.failRemoves=false;
        LittleFS.failTruncates=false;LittleFS.writeLimit=LittleFS.readLimit=LittleFS.readFailAt=SIZE_MAX;
        LittleFS.readBudget=LittleFS.writeBudget=SIZE_MAX;LittleFS.totalCapacityBytes=2097152u;
        BOOL__G__FsOk=true;func__EspLog_ResetCaches();EspBenchCodec::reset(BENCH__G__State);
    };
    const auto post=[&](const std::string &body) {
        ESP_WEB_SERVER_T__G__Server.clearArgs();ESP_WEB_SERVER_T__G__Server.args["plain"]=body;
        ESP_WEB_SERVER_T__G__Server.call("/benchlog/add",HTTP_POST);return ESP_WEB_SERVER_T__G__Server.lastCode;
    };
    const auto get=[&]() {
        ESP_WEB_SERVER_T__G__Server.clearArgs();ESP_WEB_SERVER_T__G__Server.call("/benchlog",HTTP_GET);
        return ESP_WEB_SERVER_T__G__Server.lastCode;
    };
    const auto setting=[](const std::vector<std::string> &v) {
        std::string out="# settings: ";for(unsigned i=0;i<v.size();++i){if(i)out+=',';out+=v[i];}return out+'\n';
    };
    reset();
    std::vector<std::string> values(171,"0");
    for(unsigned i=0;i<171;++i)values[i]=std::to_string(i*123u);
    values[0]="-2147483648";values[1]="4294967295";values[4]="-";values[5]="-0";values[6]="00001";
    const std::string full=setting(values);
    const std::string raw="BOTH,1,0,0,1234,2026-11-01T12:00:00.000Z,80.1,-0.5,100.125,-,0,notes keep commas\n";
    std::string expected=ESP_BENCHLOG_HEADER;expected+="# run 1 scenario=BOTH\n";expected+=full;expected+=raw;
    check(post("# run 1 scenario=BOTH\n")==200&&post(full)==200&&post(raw)==200,
          "bench codec: public ASCII POST API remains unchanged for run, settings and measurements");
    const size_t before=LittleFS.files[path].size();values[16]="200";values[18]="-5";const std::string delta=setting(values);
    check(post(delta)==200&&LittleFS.files[path].size()-before<70u,
          "bench codec: two changed settings use a bounded token delta instead of repeating 171 values");
    expected+=delta;expected+="# repeat scenario=BOTH step=1 attempt=2\n";expected+=raw;
    check(post("# repeat scenario=BOTH step=1 attempt=2\n")==200&&post(raw)==200&&get()==200&&
          ESP_WEB_SERVER_T__G__Server.lastBody==expected,
          "bench codec: CSV reconstruction is byte exact, including signed limits, missing vs zero, decimals, notes and every repeat");
    check(ESP_WEB_SERVER_T__G__Server.contentLength==expected.size()&&
          LittleFS.files[path].find("# cols:")==std::string::npos&&
          LittleFS.files[path].compare(0,4,"BNCH")==0,
          "bench codec: legend is generated on download and the response declares its exact expanded length");
    ESP_WEB_SERVER_T__G__Server.clearArgs();ESP_WEB_SERVER_T__G__Server.setArg("i","1");
    ESP_WEB_SERVER_T__G__Server.call("/benchlog",HTTP_GET);
    check(ESP_WEB_SERVER_T__G__Server.lastBody.find("\"size\":"+std::to_string(LittleFS.files[path].size()))!=std::string::npos&&
          func__EspLog_BenchLogBytes()==LittleFS.files[path].size(),
          "bench codec: info and daily-history budget count actual physical encoded bytes");
    const std::string stored=LittleFS.files[path];
    bool truncations=true;
    // [EN] Exact frame boundaries represent valid prefixes; an incomplete frame must never export as complete.
    std::vector<size_t> boundaries={EspBenchCodec::HeaderSize};size_t pos=EspBenchCodec::HeaderSize;
    while(pos<stored.size()){pos+=EspBenchCodec::u16((const uint8_t *)stored.data()+pos+2)+EspBenchCodec::FrameOverhead;boundaries.push_back(pos);}
    for(size_t cut=1;cut<stored.size();++cut) {
        LittleFS.files[path]=stored.substr(0,cut);const int status=get();
        const bool boundary=std::find(boundaries.begin(),boundaries.end(),cut)!=boundaries.end();
        truncations &= boundary?status==200:status>=400;
    }
    check(truncations,"bench codec: every byte-truncation boundary either yields a complete prefix or rejects export (no silent tail repair)");
    bool flips=true;
    for(size_t byte=0;byte<stored.size();++byte)for(unsigned bit=0;bit<8;++bit) {
        std::string bad=stored;bad[byte]^=(char)(1u<<bit);LittleFS.files[path]=bad;
        flips &= get()>=400;
    }
    auto disguised=stored;disguised[0]='#';LittleFS.files[path]=disguised;flips &= get()>=400;
    check(flips,"bench codec: every single-bit corruption in header and all fixture frames blocks CSV export");
    LittleFS.files[path]=stored.substr(0,boundaries[0])+stored.substr(boundaries[1]);
    check(get()>=400,"bench codec: removing an entire frame is detected by monotonically consecutive sequence numbers");
    LittleFS.files[path]=stored.substr(0,boundaries[1])+stored.substr(boundaries[0],boundaries[1]-boundaries[0])+stored.substr(boundaries[1]);
    check(get()>=400,"bench codec: duplicate/reordered frames are not silently consumed as extra samples");
    // [EN] Even correctly checksummed malformed frames must fail semantic validation.
    const auto rejected=[&](std::string bad) {
        if(bad.size()>=EspBenchCodec::FrameOverhead)EspBenchCodec::put32((uint8_t *)&bad[bad.size()-4u],EspBenchCodec::crc((const uint8_t *)bad.data(),bad.size()-4u));
        EspBenchCodec::State state;char text[EspBenchCodec::MaxText+1];
        return !EspBenchCodec::decode(state,(const uint8_t *)bad.data(),bad.size(),text);
    };
    std::string first=stored.substr(EspBenchCodec::HeaderSize,boundaries[1]-EspBenchCodec::HeaderSize);
    auto invalidType=first;invalidType[8]=3;
    auto invalidCount=first;EspBenchCodec::put16((uint8_t *)&invalidCount[9],EspBenchCodec::MaxText+1u);
    auto invalidSequence=first;EspBenchCodec::put32((uint8_t *)&invalidSequence[4],0u);
    check(rejected(invalidType)&&rejected(invalidCount)&&rejected(invalidSequence),
          "bench codec: semantic type, decoded-length and sequence checks do not rely on CRC alone");
    LittleFS.files[path]=stored.substr(0,stored.size()-1u);File tail=LittleFS.open(path,"r");
    const int truncated=func__Esp_BenchScan(tail);
    const bool tailDamage=truncated==-1&&BENCH__G__State.damaged&&!BENCH__G__State.ioError;tail.close();
    LittleFS.files[path]=stored;LittleFS.readFailAt=stored.size()-1u;File failedRead=LittleFS.open(path,"r");
    const int io=func__Esp_BenchScan(failedRead);failedRead.close();LittleFS.readFailAt=SIZE_MAX;
    check(tailDamage&&io==-2&&BENCH__G__State.ioError,
          "bench codec: actual tail truncation is distinguished from a short I/O read against a complete file");
    LittleFS.files[path]=stored;LittleFS.files[path].pop_back();const auto corrupt=LittleFS.files[path];
    check(post(raw)>=400&&LittleFS.files[path]==corrupt,"bench codec: append never overwrites or extends a damaged file");
    LittleFS.files[path]=stored;
    bool readErrors=true;
    for(size_t at=0;at<stored.size();++at) {
        LittleFS.readFailAt=at;readErrors &= get()>=400;
        readErrors &= post(raw)>=400&&LittleFS.files[path]==stored;
    }
    LittleFS.readFailAt=SIZE_MAX;
    check(readErrors,"bench codec: every read-failure offset refuses export/append without destructive repair");
    LittleFS.readBudget=stored.size()+EspBenchCodec::HeaderSize+EspBenchCodec::FrameHead;
    const int interrupted=get();
    check(interrupted==200&&!ESP_WEB_SERVER_T__G__Server.client().connected()&&
          ESP_WEB_SERVER_T__G__Server.lastBody.size()<ESP_WEB_SERVER_T__G__Server.contentLength,
          "bench codec: I/O failure after response headers closes a short known-length stream, never a successful partial CSV");
    LittleFS.readBudget=SIZE_MAX;
    LittleFS.failOpenCount=1;
    check(get()==503,"bench codec: failed open is not mistaken for an empty file with a successful header-only download");
    LittleFS.failOpenCount=1;ESP_WEB_SERVER_T__G__Server.clearArgs();ESP_WEB_SERVER_T__G__Server.setArg("i","1");ESP_WEB_SERVER_T__G__Server.call("/benchlog",HTTP_GET);
    check(ESP_WEB_SERVER_T__G__Server.lastCode==503,"bench codec: info cannot report size zero after an open failure");
    bool shortWrites=true;
    // [EN] Existing-file rollback and a restart/rescan at every incomplete frame byte.
    const size_t rawSize=raw.size()+EspBenchCodec::FrameOverhead;
    for(size_t limit=0;limit<rawSize;++limit) {
        LittleFS.files[path]=stored;LittleFS.writeLimit=limit;
        shortWrites &= post(raw)==503&&LittleFS.files[path]==stored;
        LittleFS.writeLimit=SIZE_MAX;EspBenchCodec::reset(BENCH__G__State);
        shortWrites &= post(raw)==200&&get()==200&&ESP_WEB_SERVER_T__G__Server.lastBody==expected+raw;
    }
    check(shortWrites,"bench codec: every partial append rolls back, returns failure and retries exactly from disk after restart");
    bool firstWrites=true;
    for(size_t limit=0;limit<EspBenchCodec::HeaderSize+rawSize;++limit) {
        LittleFS.files.clear();LittleFS.writeBudget=limit;
        firstWrites &= post(raw)==503&&!LittleFS.exists(path);
    }
    LittleFS.writeBudget=SIZE_MAX;
    check(firstWrites,"bench codec: every partial initial header/frame write is refused and cleaned up");
    LittleFS.files[path]=stored;LittleFS.writeLimit=1;LittleFS.failTruncates=true;
    const int writeFailed=post(raw);LittleFS.writeLimit=SIZE_MAX;LittleFS.failTruncates=false;
    check(writeFailed==503&&LittleFS.files[path].size()==stored.size()+1u&&get()>=400&&post(raw)>=400,
          "bench codec: failed rollback leaves explicit damage and cannot poison a later apparently successful append");
    LittleFS.files.clear();LittleFS.readBudget=0;
    const int verifyFailed=post(raw);LittleFS.readBudget=SIZE_MAX;
    check(verifyFailed==503&&get()==200&&ESP_WEB_SERVER_T__G__Server.lastBody==std::string(ESP_BENCHLOG_HEADER)+raw,
          "bench codec: a post-write verification read failure is reported, with committed bytes preserved for later inspection");
    LittleFS.failRemoves=true;ESP_WEB_SERVER_T__G__Server.clearArgs();ESP_WEB_SERVER_T__G__Server.call("/benchlog/clear",HTTP_POST);
    check(ESP_WEB_SERVER_T__G__Server.lastCode==503&&LittleFS.exists(path),"bench codec: failed clear cannot claim success");LittleFS.failRemoves=false;
    LittleFS.files[path]=std::string(ESP_BENCHLOG_HEADER)+raw;const auto legacy=LittleFS.files[path];
    check(get()==200&&ESP_WEB_SERVER_T__G__Server.lastBody==legacy&&post(raw)==409&&LittleFS.files[path]==legacy,
          "bench codec: legacy CSV remains downloadable but appends require explicit clear (no mixed formats or automatic migration)");
    ESP_WEB_SERVER_T__G__Server.clearArgs();ESP_WEB_SERVER_T__G__Server.setArg("i","1");ESP_WEB_SERVER_T__G__Server.call("/benchlog",HTTP_GET);
    check(ESP_WEB_SERVER_T__G__Server.lastBody.find("\"format\":\"legacy\"")!=std::string::npos,
          "bench codec: info identifies legacy files before the browser can start hardware capture");
    ESP_WEB_SERVER_T__G__Server.clearArgs();ESP_WEB_SERVER_T__G__Server.call("/benchlog/clear",HTTP_POST);
    check(post(delta)==200&&get()==200&&ESP_WEB_SERVER_T__G__Server.lastBody==std::string(ESP_BENCHLOG_HEADER)+delta,
          "bench codec: clear/recreate cannot reuse a previous file's settings dependency");
    // [EN] Many edits, missing values, token-width changes and mandatory checkpoint cadence.
    reset();bool checkpoints=true;std::string longCsv=ESP_BENCHLOG_HEADER;unsigned previousFull=0;
    const auto start=std::chrono::steady_clock::now();
    for(unsigned r=0;r<70;++r) {
        values[(r*13u)%171u]=r%4==0?"-":std::to_string((int)r*999-10000);
        const auto line=setting(values);const size_t oldSize=LittleFS.files[path].size();
        checkpoints &= post(line)==200;
        const uint8_t type=(uint8_t)LittleFS.files[path][(oldSize?oldSize:(size_t)EspBenchCodec::HeaderSize)+8u];
        if(type==1u){checkpoints &= r-previousFull<=16u;previousFull=r;}else checkpoints &= r-previousFull<16u;
        longCsv+=line;
    }
    checkpoints &= get()==200&&ESP_WEB_SERVER_T__G__Server.lastBody==longCsv;
    check(checkpoints,"bench codec: 70 changing settings snapshots reconstruct exactly with full checkpoints at most 16 settings apart");
    const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"  MEASURE bench settings sweep: legacy="<<longCsv.size()<<" stored="<<LittleFS.files[path].size()<<" bytes; host 70 appends+download="<<us<<" us; static workspace="<<sizeof(BENCH__G__State)+sizeof(BENCH__G__Frame)<<" bytes + shared JSON buffer (not device timing)\n";
    check(post("# run 2 scenario=BAT1\n")==200,"bench codec: run boundary is retained");
    const size_t runSize=LittleFS.files[path].size();
    check(post(setting(values))==200&&(uint8_t)LittleFS.files[path][runSize+8u]==1u,"bench codec: each run boundary forces a full settings checkpoint");
    // [EN] Unsupported lexical settings remain byte-identical raw, without numeric loss.
    const std::string fractional="# settings: 1.125,-0.0001,not-a-complete-schema\n";
    const size_t fractionOffset=LittleFS.files[path].size();
    check(post(fractional)==200&&(uint8_t)LittleFS.files[path][fractionOffset+8u]==0u&&get()==200&&
          ESP_WEB_SERVER_T__G__Server.lastBody.find(fractional)!=std::string::npos,
          "bench codec: noncanonical settings syntax uses lossless raw fallback, never conversion/rounding");
    std::vector<std::string> allChanged(171,"-2147483648");
    const size_t fullOffset=LittleFS.files[path].size();const std::string allLine=setting(allChanged);
    // [EN] All-extreme settings exceed the 1536-byte public POST limit: reject rather than truncate.
    check(allLine.size()>EspBenchCodec::MaxText&&post(allLine)==400&&LittleFS.files[path].size()==fullOffset,
          "bench codec: a settings image larger than the unchanged POST limit is rejected whole, never truncated");
    for(unsigned i=0;i<171;++i)allChanged[i]=std::to_string(i+1u);
    const auto allValues=setting(allChanged);
    check(post(allValues)==200&&(uint8_t)LittleFS.files[path][fullOffset+8u]==1u&&get()==200&&
          ESP_WEB_SERVER_T__G__Server.lastBody.find(allValues)!=std::string::npos,
          "bench codec: changing all 171 IDs falls back to a complete settings frame when a delta would be larger");
    reset();const std::string maxBody(EspBenchCodec::MaxText-1,'~');
    check(post(maxBody+'\n')==200&&get()==200&&ESP_WEB_SERVER_T__G__Server.lastBody==std::string(ESP_BENCHLOG_HEADER)+maxBody+'\n'&&
          post(maxBody+"x\n")==400&&post(std::string("bad\0body\n",9))==400&&post("bad\ttext\n")==400,
          "bench codec: max-size printable POST round-trips, while overlong/NUL/control input is rejected");
    // [EN] Freeze the cap using real production frames, not padding interpreted as valid data.
    bool cap=true;std::string logical=ESP_BENCHLOG_HEADER;logical+=maxBody+'\n';
    while(LittleFS.files[path].size()+maxBody.size()+1u+EspBenchCodec::FrameOverhead<=ESP_BENCHLOG_MAX_BYTES) {
        cap &= post(maxBody+'\n')==200;logical+=maxBody+'\n';
    }
    const auto capped=LittleFS.files[path];
    cap &= post(maxBody+'\n')==507&&LittleFS.files[path]==capped&&get()==200&&ESP_WEB_SERVER_T__G__Server.lastBody==logical;
    check(cap,"bench codec: the 100 KiB physical cap includes frame overhead and refuses whole records without altering prior data");
    reset();LittleFS.files["/foreign.dat"]=std::string(39000,'x');LittleFS.totalCapacityBytes=40960;
    check(post(raw)==507&&!LittleFS.exists(path),"bench codec: calibration still cannot overwrite a foreign file when history cleanup cannot free enough flash");
    // [EN] Frozen input from the real panel/HTTP capture workflow; allow freshly regenerated input too.
    const std::string source=__FILE__;
    const std::string defaultFixture=source.substr(0,source.find_last_of("/\\")+1u)+"fixtures/bench_capture_3step_2repeat.csv";
    const char *fixture=std::getenv("BENCH_CODEC_FIXTURE");
    if(!fixture)fixture=defaultFixture.c_str();
    {
        std::ifstream input(fixture);std::string line,fixtureCsv;bool ok=input.good();reset();unsigned rows=0;
        while(std::getline(input,line)){line+='\n';fixtureCsv+=line;ok &= post(line)==200;if(line.compare(0,5,"BOTH,")==0)++rows;}
        ok &= rows==5u&&get()==200&&ESP_WEB_SERVER_T__G__Server.lastBody==std::string(ESP_BENCHLOG_HEADER)+fixtureCsv;
        check(ok,"bench codec: actual panel/HTTP 3-step + 2-repeat fixture reconstructs every byte through production LittleFS handlers");
        std::cout<<"  MEASURE bench real capture fixture: CSV="<<fixtureCsv.size()+sizeof(ESP_BENCHLOG_HEADER)-1u<<" stored="<<LittleFS.files[path].size()<<" bytes\n";
    }
    reset();
}
