/* [EN] v1.138: exercise the production codec and every public history boundary. */
static void test_log_codec()
{
    const char *path="/evt-20261107.csv";
    LittleFS.files.clear(); LittleFS.failOpenCount=0;
    reset_clock_test_boot(); func__EspLog_ResetCaches();
    uint32_t random=0x138u;
    const auto rng=[&]() { random^=random<<13; random^=random>>17; random^=random<<5; return random; };
    EspLogCodec::State writer;
    std::vector<std::string> frames, rows;
    std::string bytes((const char *)EspLogCodec::header(),EspLogCodec::HeaderSize);
    std::string csv=ESP_LOG_HEADER;
    std::vector<size_t> ends;
    bool roundtrip=true;
    for (unsigned r=0;r<64;++r) {
        uint32_t values[EspLogCodec::Fields];
        for (unsigned j=0;j<EspLogCodec::Fields;++j) {
            values[j]=(r%4==0)?rng():writer.value[j];
        }
        values[0]=1794000000u+r*15u; values[1]=r<31?r*15: (r-31)*15;
        values[3]=0u;
        if (r<3) for (auto &v:values) v=r==1?UINT32_MAX:0u;
        uint8_t frame[EspLogCodec::MaxFrame]; char row[ESP_LOG_ROW_MAX];
        size_t n=EspLogCodec::encode(writer,values,frame);
        EspLogCodec::text(writer,row,sizeof row);
        frames.emplace_back((const char *)frame,n); rows.emplace_back(row);
        bytes+=frames.back(); csv+=rows.back(); ends.push_back(bytes.size());
    }
    LittleFS.files[path]=bytes;
    File file=LittleFS.open(path,"r"); EspLogCodec::State reader;
    roundtrip=EspLogCodec::begin(file,reader)==1;
    unsigned index=0;
    while (EspLogCodec::next(file,reader)) {
        char row[ESP_LOG_ROW_MAX]; EspLogCodec::text(reader,row,sizeof row);
        roundtrip &= index<rows.size() && rows[index]==row; ++index;
    }
    check(roundtrip && index==64 && !reader.damaged,"codec: exact all-18-field reconstruction, u32 extremes, random values and uptime reset");
    bool extremes=true;
    EspLogCodec::State extreme;
    for (auto &v:extreme.value) v=UINT32_MAX;
    extreme.value[0]=0; extreme.valid=true;
    const std::string base=std::string((const char *)EspLogCodec::header(),8);
    uint8_t out[EspLogCodec::MaxFrame];
    extreme.valid=false;
    uint32_t extremeBase[18]; memcpy(extremeBase,extreme.value,sizeof extremeBase);
    size_t full=EspLogCodec::encode(extreme,extremeBase,out);
    std::string extremeBytes=base+std::string((const char *)out,full);
    for (unsigned r=0;r<2;++r) {
        uint32_t v[18]; memcpy(v,extreme.value,sizeof v); v[0]=r?0:UINT32_MAX;
        const size_t count=EspLogCodec::encode(extreme,v,out);
        extremes &= out[3]==1u;
        extremeBytes.append((const char *)out,count);
    }
    LittleFS.files[path]=extremeBytes;
    File ef=LittleFS.open(path,"r"); EspLogCodec::State er; EspLogCodec::begin(ef,er);
    extremes &= EspLogCodec::next(ef,er) && er.value[0]==0;
    extremes &= EspLogCodec::next(ef,er) && er.value[0]==UINT32_MAX;
    extremes &= EspLogCodec::next(ef,er) && er.value[0]==0;
    check(extremes,"codec: positive and negative 33-bit ZigZag deltas reconstruct u32 boundaries exactly");
    uint32_t parsed[EspLogCodec::Fields];
    check(!EspLogCodec::parse("4294967296,0",parsed) && !EspLogCodec::parse("0",parsed) &&
          !EspLogCodec::parse("-1",parsed),"codec: malformed and overflowing CSV rejected");
    bool truncations=true;
    for (size_t cut=0;cut<=bytes.size();++cut) {
        LittleFS.files[path]=bytes.substr(0,cut);
        File f=LittleFS.open(path,"r"); EspLogCodec::State st;
        const int kind=EspLogCodec::begin(f,st);
        unsigned actual=0;
        if (kind==1) while (EspLogCodec::next(f,st)) {
            char row[ESP_LOG_ROW_MAX]; EspLogCodec::text(st,row,sizeof row);
            truncations &= actual<rows.size() && row==rows[actual]; ++actual;
        }
        const unsigned expected=(unsigned)std::count_if(ends.begin(),ends.end(),[&](size_t e) { return e<=cut; });
        truncations &= actual==expected && (cut>=8?kind==1:kind==2);
    }
    check(truncations,"codec: every byte-truncation boundary preserves exactly the completed prefix");
    bool flips=true;
    for (size_t byte=8;byte<ends[0];++byte) for (unsigned bit=0;bit<8;++bit) {
        std::string corrupt=bytes; corrupt[byte]^=(char)(1u<<bit); LittleFS.files[path]=corrupt;
        File f=LittleFS.open(path,"r"); EspLogCodec::State st; EspLogCodec::begin(f,st);
        unsigned got=0;
        while (EspLogCodec::next(f,st)) {
            char row[ESP_LOG_ROW_MAX]; EspLogCodec::text(st,row,sizeof row);
            flips &= std::find(rows.begin()+1,rows.end(),std::string(row))!=rows.end(); ++got;
        }
        flips &= st.damaged && got>0;
    }
    check(flips,"codec: every single-bit flip in first frame detected, later full checkpoint recovers without invented values");
    /* [EN] CRC-valid hostile payloads must fail semantic validation too. */
    std::vector<std::string> malformed;
    auto packet=[&](uint8_t kind,const std::string &payload) {
        std::string p; p+=(char)0xa5; p+=(char)0xc3; p+=(char)payload.size(); p+=(char)kind; p+=payload;
        const uint16_t crc=EspLogCodec::crc((const uint8_t *)p.data()+2,p.size()-2);
        p+=(char)crc; p+=(char)(crc>>8); return p;
    };
    malformed.push_back(packet(0,""));
    malformed.push_back(packet(0,std::string(90,(char)0xff)));
    malformed.push_back(packet(0,std::string(18,'\0')+"x"));
    malformed.push_back(packet(1,std::string("\0\0\x80",3)));
    malformed.push_back(packet(1,std::string("\1\0\0",3)+std::string(5,(char)0xff)));
    malformed.push_back(packet(2,std::string(18,'\0')));
    bool hostile=true;
    for (const auto &bad:malformed) {
        LittleFS.files[path]=std::string((const char *)EspLogCodec::header(),8)+frames[0]+bad+frames[1]+frames[2];
        File f=LittleFS.open(path,"r"); EspLogCodec::State st; EspLogCodec::begin(f,st);
        unsigned got=0;
        while (EspLogCodec::next(f,st)) ++got;
        hostile &= st.damaged && got>=2;
    }
    check(hostile,"codec: CRC-valid malformed lengths, masks, kinds, overlong varints and trailing payload rejected");
    std::string chain=std::string((const char *)EspLogCodec::header(),8)+frames[0];
    for (unsigned i=0;i<20;++i) chain+=packet(1,std::string(3,'\0'));
    chain+=frames[0]; LittleFS.files[path]=chain;
    File chainFile=LittleFS.open(path,"r"); EspLogCodec::State chainState;
    EspLogCodec::begin(chainFile,chainState);
    while (EspLogCodec::next(chainFile,chainState)) {}
    check(chainState.damaged && chainState.count==17u,"codec: hostile overlong delta chain cannot wrap checkpoint counter; next full row recovers");
    /* [EN] Exercise real append failure/retry with every possible partial frame. */
    bool partials=true;
    for (unsigned record : {1u,3u}) for (size_t cut=0;cut<frames[record].size();++cut) {
        LittleFS.files[path]=bytes.substr(0,ends[record-1]); BOOL__G__LogCodecCached=false;
        LittleFS.writeLimit=cut;
        partials &= !func__EspLog_AppendRow(20261107u,rows[record].c_str());
        LittleFS.writeLimit=SIZE_MAX;
        partials &= func__EspLog_AppendRow(20261107u,rows[record].c_str());
        std::string expectedPrefix=ESP_LOG_HEADER;
        for (unsigned i=0;i<=record;++i) expectedPrefix+=rows[i];
        partials &= decoded_day(path)==expectedPrefix;
    }
    for (size_t cut=0;cut<8;++cut) {
        LittleFS.files[path]=bytes.substr(0,cut); BOOL__G__LogCodecCached=false;
        partials &= func__EspLog_AppendRow(20261107u,rows[0].c_str());
        partials &= decoded_day(path)==std::string(ESP_LOG_HEADER)+rows[0];
    }
    for (size_t cut=0;cut<8;++cut) {
        LittleFS.files.erase(path); BOOL__G__LogCodecCached=false; LittleFS.writeLimit=cut;
        partials &= !func__EspLog_AppendRow(20261107u,rows[0].c_str());
        LittleFS.writeLimit=SIZE_MAX;
        partials &= func__EspLog_AppendRow(20261107u,rows[0].c_str());
        partials &= decoded_day(path)==std::string(ESP_LOG_HEADER)+rows[0];
    }
    check(partials,"codec: actual short writes at every frame byte and partial file headers retry without duplicate or fabricated rows");
    LittleFS.files[path]="EVTD\2invalid"; BOOL__G__LogCodecCached=false;
    const std::string unknown=LittleFS.files[path];
    check(!func__EspLog_AppendRow(20261107u,rows[0].c_str()) && LittleFS.files[path]==unknown,"codec: unknown header is never erased or appended to");
    /* [EN] Fresh realistic 200-row day through the production write path. */
    LittleFS.files.clear(); BOOL__G__LogCodecCached=false;
    csv=ESP_LOG_HEADER; rows.clear();
    const auto start=std::chrono::steady_clock::now();
    bool appended=true;
    for (unsigned r=0;r<200;++r) {
        EspLogCodec::State st;
        uint32_t v[18]={1794000000u+r*60u,r*60u,1024u,0u,1u,1u,2u,2u,0u,0u,11900u+r,12100u+r,0u,24000u,500u+r,510u+r,400u,410u};
        memcpy(st.value,v,sizeof v); char row[ESP_LOG_ROW_MAX]; EspLogCodec::text(st,row,sizeof row);
        appended &= func__EspLog_AppendRow(20261107u,row);
        rows.emplace_back(row); csv+=row;
    }
    const auto micros=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    check(appended && decoded_day(path)==csv,"codec: production append immediately persists all 200 exact rows");
    check(LittleFS.files[path].size()<csv.size()/2,"codec: representative synthetic 200-row day uses less than half the CSV bytes");
    std::cout<<"  MEASURE codec synthetic 200 rows: CSV="<<csv.size()<<" stored="<<LittleFS.files[path].size()<<" bytes; host append total="<<micros<<" us (not device timing)\n";
    LittleFS.files["/evt-20261108.csv"]=ESP_LOG_HEADER;
    const auto legacyStart=std::chrono::steady_clock::now();
    bool legacyOk=true;
    for (const auto &row:rows) {
        const std::string noNewline=row.substr(0,row.size()-1);
        legacyOk &= func__EspLog_AppendRow(20261108u,noNewline.c_str());
    }
    const auto legacyMicros=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-legacyStart).count();
    check(legacyOk && LittleFS.files["/evt-20261108.csv"]==csv,"codec: legacy text file still appends and exports unchanged");
    std::cout<<"  MEASURE matching legacy CSV append total="<<legacyMicros<<" us on host stubs (not device timing)\n";
    check(func__EspLog_CountRows(path)==200u,"codec: row cap/count semantics preserved");
    char tail[1024]; std::string expected;
    for (unsigned r=195;r<200;++r) expected+=rows[r];
    func__EspLog_Tail(tail,sizeof tail,20261107u,5u);
    check(tail==expected,"codec: bounded public table tail returns the newest exact five rows");
    char tiny[100]; func__EspLog_Tail(tiny,sizeof tiny,20261107u,30u);
    check(rows.back()==tiny,"codec: tail byte budget keeps whole latest rows, no partial CSV");
    ESP_WEB_SERVER_T__G__Server.clearArgs(); ESP_WEB_SERVER_T__G__Server.addArg("day","20261107");
    func__Esp_HttpEventDownload();
    check(ESP_WEB_SERVER_T__G__Server.lastCode==200 && ESP_WEB_SERVER_T__G__Server.lastBody==csv &&
          ESP_WEB_SERVER_T__G__Server.lastType=="text/csv","codec: HTTP download is full real CSV byte for byte, never compressed bytes");
    UINT32_T__G__LogDayCode=20261107u;
    func__EspLog_LoadLastSnapshot();
    check(UINT32_T__G__LogBootV24==24398u && UINT32_T__G__LogBootVlow==12099u &&
          UINT32_T__G__LogBootVhigh==12299u,"codec: boot snapshot restores the last decoded halves and derived pack");
    /* [EN] Damage remains visible on downloads and recovers at a checkpoint. */
    LittleFS.files[path][12]^=1; BOOL__G__LogCodecCached=false;
    func__Esp_HttpEventDownload();
    check(ESP_WEB_SERVER_T__G__Server.lastBody.find("# WARNING:")!=std::string::npos && UINT8_T__G__LogWriteFault,
          "codec: damaged export explicitly warns instead of claiming a complete day");
    LittleFS.files[path]=bytes.substr(0,bytes.size()-1); BOOL__G__LogCodecCached=false;
    check(func__EspLog_RepairTail(path) && LittleFS.files[path].size()==ends[62],"codec: boot repair truncates only incomplete binary suffix, never appends newline");
    LittleFS.files.clear(); func__EspLog_ResetCaches();
}
