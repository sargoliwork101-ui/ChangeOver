// Production chart sampling, checkpoint and replay regressions.
static std::string func__T_ChartBytes(const char *path)
{
    std::ifstream file(func__T_FsPath(path), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
static void func__T_ChartWrite(const char *path, const std::string &bytes)
{
    std::ofstream file(func__T_FsPath(path), std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), (std::streamsize)bytes.size());
}
static void func__T_ChartTime(uint32_t sec)
{
    func__Simple_TimeSet(sec, false);
    BOOL__G__TimeFromDisk = false;BOOL__G__BrowserTimePending = false;
}
static void func__T_ChartFixture(void)
{
    BOOL__G__FsOk = true;LittleFS.format();func__Simple_ChartsClear();
    BOOL__G__TimePairValid = false;BOOL__G__TimeBoardLive = false;
    func__T_ChartTime(1793836800u);
    UINT__T__ChartSnapshotWrites = 0u;
}
static void func__T_TestChartTriggersAndReplay(void)
{
    (void)printf("\n== نمودار: زمان یا تغییر پک یا ورودی ==\n");
    func__T_ChartFixture();const uint32_t base = func__Simple_NowSec();
    func__Simple_ChartAddTimed(50u,24000u,25000u);
    func__T_Check(UINT32_T__G__ChartSequence == 1u && !BOOL__G__ChartsDirty && !func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE).empty(), "اولین نمونه همان لحظه روی فلش ثبت می‌شود");
    func__T_ChartTime(base+300u);func__Simple_ChartAddTimed(50u,24000u,25000u);
    func__T_Check(UINT32_T__G__ChartSequence == 1u, "گذشت پنج دقیقه دیگر برای ولتاژ ثابت نمونه نمی‌سازد");
    func__T_ChartTime(base+3599u);func__Simple_ChartAddTimed(90u,24499u,25499u);
    func__T_Check(UINT32_T__G__ChartSequence == 1u, "۳۵۹۹ ثانیه و ۴۹۹ میلی‌ولت، حتی با تغییر درصد، نمونه اضافی نمی‌سازد");
    func__T_ChartTime(base+3600u);func__Simple_ChartAddTimed(50u,24000u,25000u);
    func__T_Check(UINT32_T__G__ChartSequence == 2u, "در مرز دقیق یک ساعت، ولتاژ ثابت هم ثبت می‌شود");
    func__T_ChartTime(base+3601u);func__Simple_ChartAddTimed(50u,24500u,25000u);
    func__T_Check(UINT32_T__G__ChartSequence == 3u, "افزایش دقیق ۵۰۰ میلی‌ولت ورودی قبل از یک ساعت ثبت می‌شود");
    func__T_ChartTime(base+3602u);func__Simple_ChartAddTimed(50u,24500u,25500u);
    func__T_Check(UINT32_T__G__ChartSequence == 4u, "افزایش دقیق ۵۰۰ میلی‌ولت پک هم مستقل ثبت می‌شود");
    func__T_ChartTime(base+3603u);func__Simple_ChartAddTimed(50u,24000u,25500u);
    func__T_ChartTime(base+3604u);func__Simple_ChartAddTimed(50u,24000u,25000u);
    func__T_Check(UINT32_T__G__ChartSequence == 6u, "کاهش ۵۰۰ میلی‌ولت هر دو ولتاژ هم ثبت می‌شود");
    func__T_Check(UINT__T__ChartSnapshotWrites == 0u, "برای شش نمونه فایل کامل نمودار حتی یک بار بازنویسی نمی‌شود");
    func__Simple_ChartsLoad();func__T_ChartTime(base+3605u);func__Simple_ChartAddTimed(50u,24200u,25200u);
    func__T_Check(UINT32_T__G__ChartSequence == 6u && UINT16_T__G__Chart1D_Count == 6u, "بوت مجدد نه ردیف اجباری می‌سازد نه زمان و مبنای ولتاژ قبلی را پاک می‌کند");
    // Deltas compare against the committed sample, not each incoming measurement.
    func__T_ChartTime(base+3606u);func__Simple_ChartAddTimed(50u,24400u,25400u);
    func__T_ChartTime(base+3607u);func__Simple_ChartAddTimed(50u,24500u,25400u);
    func__T_Check(UINT32_T__G__ChartSequence == 7u, "تغییرهای کوچک نسبت به آخرین نمونه ثبت‌شده جمع می‌شوند، نه فریم قبلی");
    BOOL__G__TimeFromDisk = true;BOOL__G__TimeBoardSeen = false;UINT32_T__G__TimeBootMs = (uint32_t)millis();
    func__Simple_ChartAddTimed(50u,0u,25000u);
    func__T_Check(UINT32_T__G__ChartSequence == 7u, "پیش از بازیابی ساعت در بوت، حتی اختلاف بزرگ هم ثبت نمی‌شود");
    BOOL__G__TimeFromDisk = false;
    func__T_ChartTime(base+7207u);func__Simple_ChartAddTimed(50u,24500u,25400u);
    func__T_Check(UINT32_T__G__ChartSequence == 8u, "پس از بوت زمان واقعی از آخرین ذخیره سنجیده می‌شود");
    // Every possible cut within the last record preserves exactly the committed prefix.
    const std::string before = func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE);
    func__T_ChartTime(base+10807u);func__Simple_ChartAddTimed(50u,24500u,25400u);
    const std::string complete = func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE);
    bool cutsOk = true;
    for (size_t n=before.size();n<complete.size();n++)
    {
        func__T_ChartWrite(SIMPLE_CHART_JOURNAL_FILE,complete.substr(0u,n));func__Simple_ChartsLoad();
        cutsOk = cutsOk && UINT32_T__G__ChartSequence == 8u && UINT16_T__G__Chart1D_Count == 8u;
    }
    func__T_Check(cutsOk, "قطع نوشتن در هر بایت رکورد تازه، تمام هشت نمونه قبلی را سالم نگه می‌دارد");
    func__T_ChartTime(base+14407u);func__Simple_ChartAddTimed(50u,24500u,25400u);func__Simple_ChartsLoad();
    func__T_Check(UINT32_T__G__ChartSequence == 9u && UINT16_T__G__Chart1D_Count == 9u,
                  "بعد از فایل نیمه، ادامه ذخیره با تعمیر انتها و بدون ردیف تکراری انجام می‌شود");
}
static void func__T_TestChartFailureRecovery(void)
{
    func__T_ChartFixture();const uint32_t base=func__Simple_NowSec();
    func__Simple_ChartAddTimed(50u,24000u,25000u);
    const std::string saved=func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE);
    func__T_ChartTime(base+3600u);SIM_BOOL__G__RefuseWrites=true;
    func__Simple_ChartAddTimed(50u,24000u,25000u);SIM_BOOL__G__RefuseWrites=false;
    func__T_Check(BOOL__G__ChartPending && BOOL__G__ChartsDirty && UINT32_T__G__ChartSequence == 1u && func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE)==saved,
                  "خطای فلش نمونه را ذخیره‌شده اعلام نمی‌کند، مبنا جلو نمی‌رود و نمونه منتظر تلاش مجدد می‌ماند");
    func__Simple_ChartsSave();func__Simple_ChartsLoad();
    func__T_Check(UINT32_T__G__ChartSequence == 2u && !BOOL__G__ChartPending, "تلاش مجدد همان نمونه را یک بار ثبت و بازیابی می‌کند");
    func__T_ChartTime(base+7200u);UINT__T__ChartReadFailureCountdown=2u;
    const std::string verifiedPrefix=func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE);
    func__Simple_ChartAddTimed(50u,24000u,25000u);
    func__T_Check(BOOL__G__ChartPending && UINT32_T__G__ChartSequence==2u && func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE)==verifiedPrefix,
                  "شکست بازخوانی بعد از نوشتن هم نمونه را تایید نمی‌کند و انتها به پیشوند تاییدشده برمی‌گردد");
    func__Simple_ChartsLoad();

    const std::string committed=func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE);
    bool shortWritesOk=true;
    for (size_t limit=0u;limit<12u;limit++)
    {
        func__T_ChartWrite(SIMPLE_CHART_JOURNAL_FILE,committed);func__Simple_ChartsLoad();
        func__T_ChartTime(base+7200u);SIM_SIZE_T__G__WriteLimit=limit;
        func__Simple_ChartAddTimed(50u,24000u,25000u);SIM_SIZE_T__G__WriteLimit=(size_t)-1;
        shortWritesOk = shortWritesOk && UINT32_T__G__ChartSequence == 2u && BOOL__G__ChartPending && func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE)==committed;
    }
    func__T_Check(shortWritesOk, "نوشتن نیمه رکورد در تمام طول‌های آزمایش‌شده، پیشوند سالم و نمونه معلق را حفظ می‌کند");
    func__Simple_ChartsSave();
    const uint32_t seq=UINT32_T__G__ChartSequence;
    func__T_Check(func__Simple_ChartCheckpoint(), "فشرده‌سازی دوره‌ای یک فایل کامل تاییدشده می‌سازد");
    const std::string checkpoint=func__T_ChartBytes(SIMPLE_CHARTS_FILE);
    func__T_ChartTime(base+10800u);func__Simple_ChartAddTimed(50u,24000u,25000u);
    const std::string journal=func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE);
    bool *faults[]={&SIM_BOOL__G__RefuseWrites,&BOOL__T__CredentialOpenFail,&BOOL__T__CredentialReadFail,&BOOL__T__CredentialRenameFail};
    for (bool *fault:faults)
    {
        *fault=true;bool ok=func__Simple_ChartCheckpoint();*fault=false;
        func__T_Check(!ok && func__T_ChartBytes(SIMPLE_CHARTS_FILE)==checkpoint && func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE)==journal,
                      "خطای هر مرحله تعویض فایل کامل، فایل قبلی و دفتر نمونه‌های جدید را با هم حفظ می‌کند");
    }
    // Simulate power loss after snapshot rename but before deleting its old journal.
    BOOL__T__RemoveFail=true;bool compact=func__Simple_ChartCheckpoint();BOOL__T__RemoveFail=false;
    func__Simple_ChartsLoad();
    func__T_Check(!compact && UINT32_T__G__ChartSequence==seq+1u && UINT16_T__G__Chart1D_Count==seq+1u,
                  "قطع بعد از ثبت فایل کامل و قبل از پاک‌کردن دفتر، نمونه‌ها و میانگین‌ها را دوبار حساب نمی‌کند");
    File tmp=LittleFS.open(SIMPLE_CHARTS_FILE ".tmp","w");tmp.print("torn");tmp.close();func__Simple_ChartsLoad();
    func__T_Check(UINT32_T__G__ChartSequence==seq+1u, "فایل موقت نیمه در بوت نادیده گرفته می‌شود");
    LittleFS.remove(SIMPLE_CHART_JOURNAL_FILE);func__T_ChartTime(base+14400u);func__Simple_ChartAddTimed(50u,24000u,25000u);func__Simple_ChartsLoad();
    func__T_Check(UINT32_T__G__ChartSequence==seq+2u && UINT16_T__G__Chart1D_Count==seq+2u,
                  "حذف تکی دفتر نمونه‌ها، ذخیره و بازیابی نمونه بعدی را خراب نمی‌کند");
    func__Simple_ChartsClear();func__Simple_ChartsLoad();
    func__T_Check(UINT16_T__G__Chart1D_Count==0u && !BOOL__G__ChartPending && !LittleFS.exists(SIMPLE_CHART_JOURNAL_FILE),
                  "پاک‌کردن نمودار دفتر، فایل کامل و نمونه معلق را با هم حذف می‌کند و چیزی برنمی‌گردد");
}
static void func__T_TestChartLongRanges(void)
{
    func__T_ChartFixture();const uint32_t base=func__Simple_NowSec();
    func__Simple_ChartAddTimed(50u,24000u,25000u);
    func__T_ChartTime(base+300u);func__Simple_ChartAddTimed(60u,24000u,26000u);
    func__Simple_ChartsLoad();
    func__T_ChartTime(base+600u);func__Simple_ChartAddTimed(70u,24000u,27000u);
    func__T_Check(UINT16_T__G__Chart1D_Count==3u && UINT16_T__G__Chart7D_Count==1u && UINT16_T__G__Chart1M_Count==1u &&
                  UINT32_T__G__Chart7D_Vpack[0]==26000u && UINT32_T__G__Chart1M_Vpack[0]==26000u,
                  "هفت‌روزه و سی‌روزه از همان سه نمونه میانگین می‌گیرند؛ مجموع و تعداد پس از بوت هم حفظ می‌شود");
    func__T_ChartTime(base+21600u);func__Simple_ChartAddTimed(50u,24000u,25000u);
    func__T_Check(UINT16_T__G__Chart7D_Count==2u && UINT16_T__G__Chart1M_Count==1u,
                  "مرز شش ساعت فقط بخش هفت‌روزه را عوض می‌کند، نه بخش روزانه سی‌روزه را");
    for (uint32_t day=1u;day<=95u;day++)
    {
        func__T_ChartTime(base+day*86400u);func__Simple_ChartAddTimed(50u,24000u,25000u);
        if(day%9u==0u)func__Simple_ChartsLoad();
    }
    func__Simple_ChartsLoad();
    func__T_Check(UINT16_T__G__Chart1D_Count==1u && UINT16_T__G__Chart7D_Count<=28u && UINT16_T__G__Chart1M_Count==30u && UINT16_T__G__Chart3M_Count==90u,
                  "پس از ۹۵ روز، حذف نقاط یک‌روزه خلاصه‌های ماهانه و سه‌ماهه را پاک نمی‌کند و سن داده‌ها محدود می‌ماند");
    func__T_ChartFixture();
    for (uint32_t i=0u;i<500u;i++)
    {
        func__T_ChartTime(base+i*300u);func__Simple_ChartAddTimed(50u,24000u+(i%2u)*500u,25000u);
    }
    func__Simple_ChartsLoad();
    func__T_Check(UINT16_T__G__Chart1D_Count==288u && UINT32_T__G__ChartSequence==500u,
                  "نمودار یک‌روزه واقعاً ظرفیت ۲۸۸ نمونه ناشی از تغییر ولتاژ دارد، نه فقط ۲۴ نقطه");
    const size_t snapshot=func__T_ChartBytes(SIMPLE_CHARTS_FILE).size(), journalSize=func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE).size();
    printf("CHART STORAGE: 500 samples, snapshot=%zu bytes, journal=%zu bytes, checkpoint writes=%u\n",snapshot,journalSize,UINT__T__ChartSnapshotWrites);
    func__T_Check(snapshot<=SIMPLE_CHART_FILE_MAX_BYTES && journalSize<=SIMPLE_CHART_JOURNAL_MAX_BYTES && UINT__T__ChartSnapshotWrites<20u,
                  "دفتر اندازه محدود دارد و فایل کامل در هر نمونه بازنویسی نمی‌شود");
    SIMPLE_WEB_SERVER_T__G__Server.simClear();func__Simple_HttpCharts();
    const std::string response=SIMPLE_WEB_SERVER_T__G__Server.simBody().c_str();
    func__T_Check(response.find("\"count\":288")!=std::string::npos,
                  "پاسخ واقعی نمودار بیش از ۲۵۵ نقطه را بدون سرریز شمارنده ارسال می‌کند");
    func__Simple_ChartsClear();
}

static void func__T_TestChartBatteryAndBounds(void)
{
    func__T_ChartFixture();const uint32_t base=func__Simple_NowSec();
    func__T_BatteryHello(100u);func__Simple_TimeSave();func__Simple_ChartAddTimed(50u,0u,25000u);
    bool clockOk=true;
    for(uint32_t wake=1u;wake<=540u;wake++)
    {
        func__T_BatteryBoot();func__Simple_ChartsLoad();func__T_BatteryHello(100u+wake*180u);
        clockOk=clockOk && func__Simple_NowSec()==base+wake*180u;
        func__Simple_ChartAddTimed(50u,0u,25000u);
    }
    func__T_Check(clockOk && UINT32_T__G__ChartSequence==28u,
                  "در ۵۴۰ بیداری باتری، نمونه ثابت فقط با گذشت زمان واقعی ثبت می‌شود: ۲۸ نمونه ساعتی، نه ۵۴۱ نمونه");
    func__T_Check(UINT32_T__G__LastChartSaveSec==base+97200u && UINT16_T__G__Chart1D_Count<=SIMPLE_CHART_1D_SIZE,
                  "ساعت و مبنای نمونه‌گیری پس از ۲۷ ساعت باتری و بوت‌های واقعی از فایل حفظ می‌شود");
    func__T_ChartFixture();
    for(uint32_t i=0u;i<20u;i++)
    {
        func__T_ChartTime(base+i*300u);func__Simple_ChartAddTimed(100u,UINT32_MAX,UINT32_MAX-(i%2u)*500u);
    }
    bool checkpointOk=func__Simple_ChartCheckpoint();func__Simple_ChartsLoad();
    func__T_Check(checkpointOk && UINT16_T__G__Chart1D_Count==20u && UINT32_T__G__Chart7D_Vin[0]==UINT32_MAX &&
                  SIMPLE_CHART_BUCKET_T__G__Buckets[0].count==20u,
                  "جمع‌های ۶۴ بیتی بازه‌های بلند بدون سرریز و بدون محدودیت ۳۵بیتی خواننده لاگ ذخیره و بازیابی می‌شوند");
    std::string good=func__T_ChartBytes(SIMPLE_CHARTS_FILE), bad=good;
    bad[12]=(char)255;bad[13]=(char)127; // CRC-valid oversized ring count
    uint16_t crc=SimpleLogCodec::crc((const uint8_t*)bad.data(),bad.size()-2u);
    bad[bad.size()-2u]=(char)crc;bad[bad.size()-1u]=(char)(crc>>8);
    func__T_ChartWrite(SIMPLE_CHARTS_FILE,bad);func__Simple_ChartsLoad();
    func__T_Check(UINT16_T__G__Chart1D_Count==0u && UINT16_T__G__Chart7D_Count==0u && UINT16_T__G__Chart3M_Count==0u,
                  "فایل CRC-درست ولی بدشکل پیش از انتشار هر کدام از بازه‌ها یکجا رد می‌شود");
    func__T_ChartWrite(SIMPLE_CHARTS_FILE,good);func__Simple_ChartsLoad();
    const std::string oldSnapshot=func__T_ChartBytes(SIMPLE_CHARTS_FILE);
    SIM_SIZE_T__G__WriteLimit=16u;bool shortSnapshot=func__Simple_ChartCheckpoint();SIM_SIZE_T__G__WriteLimit=(size_t)-1;
    func__T_Check(!shortSnapshot && func__T_ChartBytes(SIMPLE_CHARTS_FILE)==oldSnapshot,
                  "نوشتن نیمه فایل کامل نیز فایل سالم قبلی را جایگزین نمی‌کند");
    const uint32_t seq=UINT32_T__G__ChartSequence;
    func__T_ChartTime(base+6000u);BOOL__T__ChartJournalOpenFail=true;func__Simple_ChartAddTimed(100u,UINT32_MAX,UINT32_MAX);
    BOOL__T__ChartJournalOpenFail=false;
    func__T_Check(BOOL__G__ChartPending && UINT32_T__G__ChartSequence==seq, "بازنشدن دفتر، مبنای نمونه را جلو نمی‌برد");
    func__Simple_ChartsSave();func__Simple_ChartsLoad();
    func__T_Check(UINT32_T__G__ChartSequence==seq+1u, "پس از رفع خطا نمونه معلق ذخیره و قابل بازیابی می‌شود");
    // Reject malicious numeric deltas before signed arithmetic can overflow.
    uint8_t frame[56]={0xa6u,0xc4u,0u,1u};size_t n=4u;uint32_t values[5];
    n+=SimpleLogCodec::put(34359738367ULL,frame+n);
    for(unsigned i=1;i<5u;i++)n+=SimpleLogCodec::put(0u,frame+n);
    frame[2]=(uint8_t)(n-4u);crc=SimpleLogCodec::crc(frame+2u,n-2u);frame[n++]=(uint8_t)crc;frame[n++]=(uint8_t)(crc>>8);
    func__T_Check(!func__Simple_ChartDecode(frame,n,values), "دلتا خارج از بازه عددی حتی با CRC سالم رد می‌شود");
    const size_t total=SIM_SIZE_T__G__FsTotalBytes;FSInfo info;LittleFS.info(info);
    const std::string prefix=func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE);
    const uint32_t previous=UINT32_T__G__ChartSequence;
    SIM_SIZE_T__G__FsTotalBytes=info.usedBytes+8u;
    func__T_ChartTime(base+9600u);func__Simple_ChartAddTimed(100u,UINT32_MAX,UINT32_MAX);
    func__T_Check(BOOL__G__ChartPending && UINT32_T__G__ChartSequence==previous && func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE)==prefix,
                  "کمبود فضای واقعی حافظه پیش از نوشتن شناسایی می‌شود؛ سابقه سالم و نمونه معلق باقی می‌مانند");
    SIM_SIZE_T__G__FsTotalBytes=total;func__Simple_ChartsSave();func__Simple_ChartsLoad();
    func__T_Check(UINT32_T__G__ChartSequence==previous+1u, "با آزادشدن فضا ذخیره بدون پاک‌کردن کل سابقه ادامه پیدا می‌کند");
    func__Simple_ChartsClear();
}

static void func__T_TestChartNinetyDayBudget(void)
{
    func__T_ChartFixture();const uint32_t base=func__Simple_NowSec();
    for(uint32_t i=0u;i<=90u*288u;i++)
    {
        func__T_ChartTime(base+i*300u);func__Simple_ChartAddTimed(50u,24000u,25000u);
    }
    func__Simple_ChartsLoad();
    func__T_Check(UINT32_T__G__ChartSequence==2161u && UINT16_T__G__Chart1D_Count==24u &&
                  UINT16_T__G__Chart7D_Count==28u && UINT16_T__G__Chart1M_Count==30u && UINT16_T__G__Chart3M_Count==90u,
                  "۲٬۱۶۱ نمونه ساعتی از ۲۵٬۹۲۱ بررسی در ۹۰ روز، هر چهار بازه را با ظرفیت درست و مستقل از حذف داده‌های روزانه نگه می‌دارد");
    size_t chartBytes=func__T_ChartBytes(SIMPLE_CHARTS_FILE).size()+func__T_ChartBytes(SIMPLE_CHART_JOURNAL_FILE).size();
    const size_t oldTotal=SIM_SIZE_T__G__FsTotalBytes;SIM_SIZE_T__G__FsTotalBytes=64000u;
    const uint32_t today=func__Simple_NowSec()/86400u;
    for(uint32_t day=0u;day<90u;day++)
    {
        char path[SIMPLE_LOG_PATH_MAX];func__SimpleLog_PathOf(path,func__SimpleLog_DateCode(today-day));
        func__T_ChartWrite(path,std::string(285u,'x'));
    }
    func__T_ChartWrite("/chart-budget-other",std::string(8000u,'x'));
    func__SimpleLog_ScanDays(true);func__SimpleLog_TrimDays();FSInfo info;LittleFS.info(info);
    printf("CHART 90 DAYS: chart files=%zu bytes; combined with 90 log days and 8000 other bytes=%zu / %zu\n",chartBytes,info.usedBytes,info.totalBytes);
    func__T_Check(chartBytes<6000u && info.usedBytes<64000u && UINT8_T__G__LogIndexCount==90u,
                  "نمودار و ۹۰ روز لاگ عادی و هشت کیلوبایت فایل دیگر، هم‌زمان در فلش ۶۴ کیلوبایتی جا می‌شوند");
    SIM_SIZE_T__G__FsTotalBytes=oldTotal;LittleFS.format();func__Simple_ChartsClear();
}
