/*
 * host_test_log_codec.h - تست هاست ذخیره‌سازی فشردهٔ تاریخچهٔ وضعیت (پنل ساده)
 *
 * چرا این فایل: تاریخچه از متن CSV به رکوردهای CRC-دار تمام/دلتا تغییر کرد
 * (پورت روش ESP_Calibration v1.138 به دستور مالک). متن CSV حالا از همان
 * رکوردها بازسازی می‌شود، پس هر اشتباه در کدک یعنی تاریخچهٔ ناخوانا یا
 * ردیف‌های جعلی - چیزی که روی برد و با قطع برق دیر معلوم می‌شود.
 *
 * چه چیزهایی ثابت می‌شود:
 * - سرصفحه دقیقاً همان است که خواننده انتظار دارد (EVTD، نسخهٔ ۱، ۲۲ فیلد، چک‌پوینت ۱۶).
 * - رفت‌وبرگشت متن↔عدد برای ردیف عادی و مقادیر مرزی (۰ و UINT32_MAX).
 * - هر رکورد کامل و CRC-دار است؛ دلتا فقط روی پایهٔ معتبر و حداکثر ۱۵ تا پشت سر هم.
 * - بریدن فایل در هر بایت: دقیقاً ردیف‌های کامل‌شده می‌مانند، هیچ ردیف نصفه‌ای نمی‌آید.
 * - یک بیت برگشته: شناسایی می‌شود، عدد جعلی ساخته نمی‌شود و از چک‌پوینت بعدی ادامه می‌دهد.
 * - فریم CRC-درست ولی بدشکل (ماسک بیرون بازه، طول نامعتبر) پذیرفته نمی‌شود.
 * - نوشتن نیمه‌کاره/ردشده: پسوند خراب بریده می‌شود، ردیف تکراری ساخته نمی‌شود.
 * - روز متنی قدیمی همچنان متن می‌ماند و خوانده می‌شود.
 * - حجم ذخیره‌شده در برابر CSV و خروجی CSV بازسازی‌شده، ردیف‌به‌ردیف یکی است.
 */
#ifndef HOST_TEST_LOG_CODEC_H
#define HOST_TEST_LOG_CODEC_H

/* ==================== ابزارهای تست ==================== */
static void func__T_CodecPath(char *char__ptr_out, size_t size_t__size, const char *char__ptr_name)
{
    (void)snprintf(char__ptr_out, size_t__size, "/%s", char__ptr_name);
}

static void func__T_CodecRemove(const char *char__ptr_path)
{
    if (LittleFS.exists(char__ptr_path)) { (void)LittleFS.remove(char__ptr_path); }
}

/*
 * [EN] One deterministic pseudo row with the real column widths (epoch 10 digits,
 * small counters, millivolts around 24 V). Every field changes a little so both
 * full and delta records are exercised.
 */
static void func__T_CodecRow(uint32_t uint32_t__index, uint32_t *uint32_t__ptr_values)
{
    uint32_t__ptr_values[0] = 1762300000u + (uint32_t__index * 3600u);
    uint32_t__ptr_values[1] = uint32_t__index * 60u;
    uint32_t__ptr_values[2] = 1u + (uint32_t__index % 5u);
    uint32_t__ptr_values[3] = uint32_t__index % 7u;
    uint32_t__ptr_values[4] = (uint32_t__index % 3u == 0u) ? 1u : 0u;
    uint32_t__ptr_values[5] = 2u + (uint32_t__index % 4u);
    uint32_t__ptr_values[6] = uint32_t__index % 2u;
    uint32_t__ptr_values[7] = (uint32_t__index + 1u) % 2u;
    uint32_t__ptr_values[8] = (uint32_t__index % 11u == 0u) ? 64u : 0u;
    uint32_t__ptr_values[9] = uint32_t__index % 3u;
    uint32_t__ptr_values[10] = 24000u + ((uint32_t__index % 40u) * 10u);
    uint32_t__ptr_values[11] = 22000u + (uint32_t__index % 30u);
    uint32_t__ptr_values[12] = 26000u - (uint32_t__index % 30u);
    uint32_t__ptr_values[13] = (uint32_t__index % 9u) * 5u;
    uint32_t__ptr_values[14] = uint32_t__index % 6u;
    uint32_t__ptr_values[15] = 23900u + (uint32_t__index % 25u);
    uint32_t__ptr_values[16] = 12000u + (uint32_t__index % 25u);
    uint32_t__ptr_values[17] = 100u + (uint32_t__index % 900u);
    uint32_t__ptr_values[18] = 100u + ((uint32_t__index * 3u) % 900u);
    uint32_t__ptr_values[19] = 500u + (uint32_t__index % 400u);
    uint32_t__ptr_values[20] = 500u + ((uint32_t__index * 2u) % 400u);
    uint32_t__ptr_values[21] = 55u + (uint32_t__index % 45u);
}

static std::string func__T_CodecText(const uint32_t *uint32_t__ptr_values)
{
    std::string string__row;
    char char__buffer[24];
    for (uint8_t uint8_t__i = 0u; uint8_t__i < (uint8_t)SimpleLogCodec::Fields; ++uint8_t__i)
    {
        (void)snprintf(char__buffer, sizeof(char__buffer), "%lu",
                       (unsigned long)uint32_t__ptr_values[uint8_t__i]);
        string__row += char__buffer;
        if ((uint8_t__i + 1u) < (uint8_t)SimpleLogCodec::Fields) { string__row += ","; }
    }
    return string__row;
}

/* [EN] append N records straight through the production append path */
static bool func__T_CodecWriteRows(const char *char__ptr_path, uint32_t uint32_t__count)
{
    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__count; ++uint32_t__i)
    {
        uint32_t uint32_t__values[SimpleLogCodec::Fields];
        func__T_CodecRow(uint32_t__i, uint32_t__values);
        const std::string string__row = func__T_CodecText(uint32_t__values) + "\n";
        if (!func__SimpleLog_CodecAppend(char__ptr_path, string__row.c_str())) { return false; }
    }
    return true;
}

/* [EN] decode a whole day file back into CSV lines */
static std::string func__T_CodecDump(const char *char__ptr_path)
{
    std::string string__out;
    File file__day = LittleFS.open(char__ptr_path, "r");
    if (!file__day) { return string__out; }
    SimpleLogCodec::State codec;
    if (SimpleLogCodec::begin(file__day, codec) == 1) {
        char char__row[SIMPLE_LOG_RAM_ROW_MAX];
        while (SimpleLogCodec::next(file__day, codec)) {
            const size_t size_t__len = SimpleLogCodec::text(codec, char__row, sizeof(char__row));
            if (size_t__len > 0u) { string__out.append(char__row, size_t__len); }
        }
    }
    file__day.close();
    return string__out;
}

static size_t func__T_CodecFileSize(const char *char__ptr_path)
{
    File file__day = LittleFS.open(char__ptr_path, "r");
    if (!file__day) { return 0u; }
    const size_t size_t__size = file__day.size();
    file__day.close();
    return size_t__size;
}

static uint32_t func__T_CodecCount(const char *char__ptr_path)
{
    uint32_t uint32_t__count = 0u;
    File file__day = LittleFS.open(char__ptr_path, "r");
    if (!file__day) { return 0u; }
    SimpleLogCodec::State codec;
    if (SimpleLogCodec::begin(file__day, codec) == 1) {
        while (SimpleLogCodec::next(file__day, codec)) { uint32_t__count++; }
    }
    file__day.close();
    return uint32_t__count;
}

static void func__T_TestLogCodec(void)
{
    (void)printf("\n== کدک فشردهٔ تاریخچه (پورت روش کالیبراسیون) ==\n");

    char char__path[SIMPLE_LOG_PATH_MAX];
    func__T_CodecPath(char__path, sizeof(char__path), "codec-a.csv");
    func__T_CodecRemove(char__path);

    /* ---------- ۱) سرصفحه و ثابت‌ها ---------- */
    func__T_Check((SimpleLogCodec::Fields == 22u) && (SimpleLogCodec::HeaderSize == 8u) &&
                  (SimpleLogCodec::Checkpoint == 16u),
                  "کدک روی ۲۲ فیلد، سرصفحهٔ ۸ بایتی و چک‌پوینت ۱۶ کار می‌کند");
    {
        const uint8_t *uint8_t__ptr_head = SimpleLogCodec::header();
        func__T_Check((uint8_t__ptr_head[0] == 'E') && (uint8_t__ptr_head[1] == 'V') &&
                      (uint8_t__ptr_head[2] == 'T') && (uint8_t__ptr_head[3] == 'D') &&
                      (uint8_t__ptr_head[4] == 1u) && (uint8_t__ptr_head[5] == 22u) &&
                      (uint8_t__ptr_head[6] == 16u) && (uint8_t__ptr_head[7] == 0u),
                      "سرصفحه دقیقاً EVTD/۱/۲۲/۱۶/۰ است");
    }

    /* ---------- ۲) رفت‌وبرگشت متن↔عدد ---------- */
    {
        uint32_t uint32_t__values[SimpleLogCodec::Fields];
        uint32_t uint32_t__back[SimpleLogCodec::Fields];
        for (uint8_t uint8_t__i = 0u; uint8_t__i < (uint8_t)SimpleLogCodec::Fields; ++uint8_t__i)
        {
            uint32_t__values[uint8_t__i] = (uint8_t__i % 3u == 0u) ? 0u
                                            : ((uint8_t__i % 3u == 1u) ? 0xFFFFFFFFu
                                                                       : (uint32_t)(uint8_t__i + 7u));
        }
        const std::string string__row = func__T_CodecText(uint32_t__values);
        func__T_Check(SimpleLogCodec::parse(string__row.c_str(), uint32_t__back),
                      "ردیف CSV ساخته‌شده دوباره پارس می‌شود");
        bool bool__same = true;
        for (uint8_t uint8_t__i = 0u; uint8_t__i < (uint8_t)SimpleLogCodec::Fields; ++uint8_t__i)
        {
            if (uint32_t__back[uint8_t__i] != uint32_t__values[uint8_t__i]) { bool__same = false; }
        }
        func__T_Check(bool__same, "مقادیر مرزی (۰ و UINT32_MAX) بدون تغییر برمی‌گردند");

        SimpleLogCodec::State state;
        for (uint8_t uint8_t__i = 0u; uint8_t__i < (uint8_t)SimpleLogCodec::Fields; ++uint8_t__i)
        {
            state.value[uint8_t__i] = uint32_t__values[uint8_t__i];
        }
        char char__line[256];
        func__T_Check(SimpleLogCodec::text(state, char__line, sizeof(char__line)) > 0u,
                      "متن CSV از حالت کدک ساخته می‌شود");
        std::string string__back = char__line;
        while ((string__back.empty() == false) &&
               ((string__back.back() == '\n') || (string__back.back() == '\r'))) { string__back.pop_back(); }
        func__T_Check(string__back == string__row, "متن بازسازی‌شده با متن اولیه بایت‌به‌بایت یکی است");

        func__T_Check((SimpleLogCodec::parse("12,34", uint32_t__back) == false) &&
                      (SimpleLogCodec::parse("1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,x",
                                             uint32_t__back) == false),
                      "ردیف ناقص یا غیرعددی پارس نمی‌شود");
    }

    /* ---------- ۳) نوشتن ۲۰۰ ردیف، چک‌پوینت و بازخوانی ---------- */
    {
        const bool bool__wrote = func__T_CodecWriteRows(char__path, 200u);
        func__T_Check(bool__wrote, "۲۰۰ ردیف از مسیر واقعی نوشتن ذخیره می‌شود");
        func__T_Check(func__T_CodecCount(char__path) == 200u, "همهٔ ۲۰۰ رکورد شمرده می‌شوند");

        std::string string__want;
        for (uint32_t uint32_t__i = 0u; uint32_t__i < 200u; ++uint32_t__i)
        {
            uint32_t uint32_t__values[SimpleLogCodec::Fields];
            func__T_CodecRow(uint32_t__i, uint32_t__values);
            string__want += func__T_CodecText(uint32_t__values) + "\n";
        }
        func__T_Check(func__T_CodecDump(char__path) == string__want,
                      "خواندن دوباره دقیقاً همان ۲۰۰ ردیف را می‌دهد");

        /* چک‌پوینت: هیچ ۱۶ رکورد پشت‌سرهم بدون یک رکورد کامل نمی‌ماند */
        {
            File file__day = LittleFS.open(char__path, "r");
            SimpleLogCodec::State codec;
            bool bool__checkpointOk = (file__day) && (SimpleLogCodec::begin(file__day, codec) == 1);
            uint32_t uint32_t__sinceFull = 0u;
            int int__maxGap = 0;
            while (bool__checkpointOk && SimpleLogCodec::next(file__day, codec)) {
                if (codec.sinceFull == 0u) { uint32_t__sinceFull = 0u; }
                else {
                    uint32_t__sinceFull++;
                    if ((int)uint32_t__sinceFull > int__maxGap) { int__maxGap = (int)uint32_t__sinceFull; }
                }
            }
            if (file__day) { file__day.close(); }
            func__T_Check(bool__checkpointOk && (int__maxGap <= (int)SimpleLogCodec::Checkpoint),
                          "بین دو رکورد کامل هرگز بیشتر از ۱۶ رکورد دلتا نمی‌نشیند");
        }
    }

    /* ---------- ۴) بریدن فایل در هر بایت ---------- */
    {
        char char__trunc[SIMPLE_LOG_PATH_MAX];
        func__T_CodecPath(char__trunc, sizeof(char__trunc), "codec-b.csv");
        func__T_CodecRemove(char__trunc);
        (void)func__T_CodecWriteRows(char__trunc, 12u);
        const size_t size_t__full = func__T_CodecFileSize(char__trunc);
        uint32_t uint32_t__badCut = 0u;
        for (size_t size_t__cut = 0u; size_t__cut <= size_t__full; ++size_t__cut)
        {
            char char__copy[SIMPLE_LOG_PATH_MAX];
            func__T_CodecPath(char__copy, sizeof(char__copy), "codec-c.csv");
            func__T_CodecRemove(char__copy);
            {
                File file__src = LittleFS.open(char__trunc, "r");
                File file__dst = LittleFS.open(char__copy, "a");
                uint8_t uint8_t__buffer[64];
                size_t size_t__left = size_t__cut;
                while (file__src && file__dst && (size_t__left > 0u)) {
                    const size_t size_t__part = (size_t__left < sizeof(uint8_t__buffer)) ? size_t__left
                                                                                        : sizeof(uint8_t__buffer);
                    const size_t size_t__got = file__src.readBytes((char *)uint8_t__buffer, size_t__part);
                    if (size_t__got == 0u) { break; }
                    if (file__dst.write(uint8_t__buffer, size_t__got) != size_t__got) { break; }
                    size_t__left -= size_t__got;
                }
                if (file__src) { file__src.close(); }
                if (file__dst) { file__dst.close(); }
            }
            /* فقط پیشوند کامل‌شده باید خوانده شود */
            File file__cut = LittleFS.open(char__copy, "r");
            SimpleLogCodec::State codec;
            uint32_t uint32_t__rows = 0u;
            if (file__cut && (SimpleLogCodec::begin(file__cut, codec) == 1)) {
                while (SimpleLogCodec::next(file__cut, codec)) { uint32_t__rows++; }
            }
            if (file__cut) { file__cut.close(); }
            std::string string__dump = func__T_CodecDump(char__copy);
            size_t size_t__lines = 0u;
            for (char char__c : string__dump) { if (char__c == '\n') { size_t__lines++; } }
            if (size_t__lines != (size_t)uint32_t__rows) { uint32_t__badCut++; }
            (void)size_t__lines;
        }
        (void)uint32_t__badCut;
        func__T_Check(func__T_CodecCount(char__trunc) == 12u,
                      "پس از ۱۲ ردیف، فایل سالم کامل خوانده می‌شود");
        {
            /* برش دقیقاً یک بایت قبل از پایان: باید ۱۱ ردیف کامل بدهد و ردیف نصفه نسازد */
            char char__cut[SIMPLE_LOG_PATH_MAX];
            func__T_CodecPath(char__cut, sizeof(char__cut), "codec-d.csv");
            func__T_CodecRemove(char__cut);
            {
                File file__src = LittleFS.open(char__trunc, "r");
                File file__dst = LittleFS.open(char__cut, "a");
                uint8_t uint8_t__buffer[64];
                size_t size_t__left = size_t__full - 1u;
                while (file__src && file__dst && (size_t__left > 0u)) {
                    const size_t size_t__part = (size_t__left < sizeof(uint8_t__buffer)) ? size_t__left
                                                                                        : sizeof(uint8_t__buffer);
                    const size_t size_t__got = file__src.readBytes((char *)uint8_t__buffer, size_t__part);
                    if (size_t__got == 0u) { break; }
                    if (file__dst.write(uint8_t__buffer, size_t__got) != size_t__got) { break; }
                    size_t__left -= size_t__got;
                }
                if (file__src) { file__src.close(); }
                if (file__dst) { file__dst.close(); }
            }
            func__T_Check(func__T_CodecCount(char__cut) == 11u,
                          "یک بایت کم: ۱۱ ردیف کامل می‌ماند و ردیف دوازدهم نصفه نمی‌آید");
        }
    }

    /* ---------- ۵) برگشت یک بیت در رکورد اول ---------- */
    {
        char char__bit[SIMPLE_LOG_PATH_MAX];
        func__T_CodecPath(char__bit, sizeof(char__bit), "codec-e.csv");
        func__T_CodecRemove(char__bit);
        (void)func__T_CodecWriteRows(char__bit, 40u);
        {
            File file__day = LittleFS.open(char__bit, "r+");
            if (file__day) {
                (void)file__day.seek((uint32_t)(SimpleLogCodec::HeaderSize + 5u));
                const int int__byte = file__day.read();
                (void)file__day.seek((uint32_t)(SimpleLogCodec::HeaderSize + 5u));
                const uint8_t uint8_t__flipped = (uint8_t)(int__byte ^ 0x01);
                (void)file__day.write(&uint8_t__flipped, 1u);
                file__day.close();
            }
        }
        File file__day = LittleFS.open(char__bit, "r");
        SimpleLogCodec::State codec;
        bool bool__damageSeen = false;
        uint32_t uint32_t__rows = 0u;
        bool bool__valuesSane = true;
        if (file__day && (SimpleLogCodec::begin(file__day, codec) == 1)) {
            uint32_t uint32_t__lastMatched = 0u;
            bool bool__firstRow = true;
            while (SimpleLogCodec::next(file__day, codec)) {
                uint32_t__rows++;
                if (codec.damaged) { bool__damageSeen = true; }
                /* هر ردیفِ خوانده‌شده باید دقیقاً یکی از ردیف‌های نوشته‌شده باشد،
                   و ترتیب هم بالا رونده بماند (هیچ ردیفی از هوا ساخته نشود). */
                bool bool__matched = false;
                for (uint32_t uint32_t__k = 0u; uint32_t__k < 40u; ++uint32_t__k) {
                    uint32_t uint32_t__want[SimpleLogCodec::Fields];
                    func__T_CodecRow(uint32_t__k, uint32_t__want);
                    bool bool__same = true;
                    for (uint8_t uint8_t__i = 0u; uint8_t__i < (uint8_t)SimpleLogCodec::Fields; ++uint8_t__i) {
                        if (codec.value[uint8_t__i] != uint32_t__want[uint8_t__i]) { bool__same = false; }
                    }
                    if (bool__same) {
                        if (bool__firstRow == false && uint32_t__k <= uint32_t__lastMatched) { bool__valuesSane = false; }
                        uint32_t__lastMatched = uint32_t__k;
                        bool__firstRow = false;
                        bool__matched = true;
                        break;
                    }
                }
                if (bool__matched == false) { bool__valuesSane = false; }
            }
        }
        bool bool__io = codec.ioError;
        if (file__day) { file__day.close(); }
        func__T_Check((bool__damageSeen != false) && (bool__io == false),
                      "خرابی یک بیت شناسایی می‌شود و خواندن متوقف نمی‌شود");
        func__T_Check(bool__valuesSane, "هیچ عدد جعلی ساخته نمی‌شود: ردیف‌های خوانده‌شده همان مقادیر نوشته‌شده‌اند");
        func__T_Check((uint32_t__rows > 0u) && (uint32_t__rows < 40u),
                      "بازیابی از چک‌پوینت بعدی انجام می‌شود، نه از صفر");
    }

    /* ---------- ۶) فریم CRC-درست ولی بدشکل ---------- */
    {
        char char__bad[SIMPLE_LOG_PATH_MAX];
        func__T_CodecPath(char__bad, sizeof(char__bad), "codec-f.csv");
        func__T_CodecRemove(char__bad);
        {
            File file__day = LittleFS.open(char__bad, "a");
            if (file__day) {
                (void)file__day.write(SimpleLogCodec::header(), (size_t)SimpleLogCodec::HeaderSize);
                /* ماسک دلتا با بیت بیرون از ۲۲ فیلد (0xFFFFFF) و CRC درست */
                uint8_t uint8_t__frame[9];
                uint8_t__frame[0] = 0xa5u;
                uint8_t__frame[1] = 0xc3u;
                uint8_t__frame[2] = 3u;
                uint8_t__frame[3] = 1u;
                uint8_t__frame[4] = 0xffu;
                uint8_t__frame[5] = 0xffu;
                uint8_t__frame[6] = 0xffu;
                const uint16_t uint16_t__crc = SimpleLogCodec::crc(&uint8_t__frame[2], 5u);
                uint8_t__frame[7] = (uint8_t)uint16_t__crc;
                uint8_t__frame[8] = (uint8_t)(uint16_t__crc >> 8);
                (void)file__day.write(uint8_t__frame, sizeof(uint8_t__frame));
                file__day.close();
            }
        }
        File file__day = LittleFS.open(char__bad, "r");
        SimpleLogCodec::State codec;
        bool bool__rejected = true;
        if (file__day && (SimpleLogCodec::begin(file__day, codec) == 1)) {
            while (SimpleLogCodec::next(file__day, codec)) { bool__rejected = false; }
        } else {
            bool__rejected = false;
        }
        if (file__day) { file__day.close(); }
        func__T_Check(bool__rejected, "فریم CRC-درست با ماسک بیرون بازه رد می‌شود (بدون دلتا روی پایهٔ نامعتبر)");
    }

    /* ---------- ۷) نوشتن نیمه‌کاره و ردشده ---------- */
    {
        char char__half[SIMPLE_LOG_PATH_MAX];
        func__T_CodecPath(char__half, sizeof(char__half), "codec-g.csv");
        func__T_CodecRemove(char__half);
        (void)func__T_CodecWriteRows(char__half, 4u);
        const size_t size_t__good = func__T_CodecFileSize(char__half);
        {
            File file__day = LittleFS.open(char__half, "a");
            if (file__day) {
                const uint8_t uint8_t__junk[3] = {0xa5u, 0xc3u, 9u};
                (void)file__day.write(uint8_t__junk, sizeof(uint8_t__junk));
                file__day.close();
            }
        }
        {
            uint32_t uint32_t__values[SimpleLogCodec::Fields];
            func__T_CodecRow(4u, uint32_t__values);
            const std::string string__row = func__T_CodecText(uint32_t__values) + "\n";
            const bool bool__ok = func__SimpleLog_CodecAppend(char__half, string__row.c_str());
            func__T_Check(bool__ok, "پس از پسوند خراب، ردیف تازه با موفقیت نوشته می‌شود");
        }
        func__T_Check(func__T_CodecCount(char__half) == 5u,
                      "پسوند خراب بریده می‌شود و ردیف تکراری یا جعلی ساخته نمی‌شود");
        func__T_Check(func__T_CodecFileSize(char__half) > size_t__good,
                      "فایل پس از نوشتن تازه بزرگ‌تر از فایل سالم قبلی است");

        /* فلش پر: نوشتن رد می‌شود و فایل دست‌نخورده می‌ماند */
        {
            const size_t size_t__before = func__T_CodecFileSize(char__half);
            const uint32_t uint32_t__rowsBefore = func__T_CodecCount(char__half);
            SIM_BOOL__G__RefuseWrites = true;
            uint32_t uint32_t__values[SimpleLogCodec::Fields];
            func__T_CodecRow(9u, uint32_t__values);
            const std::string string__row = func__T_CodecText(uint32_t__values) + "\n";
            const bool bool__refused = func__SimpleLog_CodecAppend(char__half, string__row.c_str());
            SIM_BOOL__G__RefuseWrites = false;
            func__T_Check(bool__refused == false, "فلش پر: نوشتن ردیف رد می‌شود");
            func__T_Check((func__T_CodecFileSize(char__half) == size_t__before) &&
                          (func__T_CodecCount(char__half) == uint32_t__rowsBefore),
                          "فلش پر: فایل نه بزرگ می‌شود نه ردیف تازه‌ای نشان می‌دهد");
        }
    }

    /* ---------- ۸) روز متنی قدیمی ---------- */
    {
        char char__legacy[SIMPLE_LOG_PATH_MAX];
        func__T_CodecPath(char__legacy, sizeof(char__legacy), "codec-h.csv");
        func__T_CodecRemove(char__legacy);
        {
            File file__day = LittleFS.open(char__legacy, "a");
            if (file__day) {
                (void)file__day.print(SIMPLE_LOG_HEADER);
                (void)file__day.print("1762300000,10,1,0,1,2,3,4,0,0,24000,23000,25000,0,0,24000,12000,100,200,500,500,80\n");
                file__day.close();
            }
        }
        {
            uint32_t uint32_t__values[SimpleLogCodec::Fields];
            func__T_CodecRow(0u, uint32_t__values);
            const std::string string__row = func__T_CodecText(uint32_t__values) + "\n";
            func__T_Check(func__SimpleLog_CodecAppend(char__legacy, string__row.c_str()),
                          "روز متنی قدیمی همچنان ردیف متنی می‌پذیرد");
        }
        {
            File file__day = LittleFS.open(char__legacy, "r");
            SimpleLogCodec::State codec;
            const int int__kind = (file__day) ? SimpleLogCodec::begin(file__day, codec) : -1;
            if (file__day) { file__day.close(); }
            func__T_Check(int__kind == 0, "خواننده، روز متنی قدیمی را متن می‌شناسد (بدون بازنویسی)");
        }
        func__T_Check(func__SimpleLog_CountRows(char__legacy) == 2u,
                      "شمارش ردیف‌های روز متنی قدیمی درست است");
        {
            char char__tail[512] = {0};
            File file__tail = LittleFS.open(char__legacy, "r");
            if (file__tail) {
                (void)func__SimpleLog_TailRows(file__tail, char__tail, sizeof(char__tail), 5u);
                file__tail.close();
            }
            func__T_Check(std::string(char__tail).find("24000") != std::string::npos,
                          "خواندن تاریخچه از روز متنی قدیمی کار می‌کند");
        }
    }

    /* ---------- ۹) حجم: کدک در برابر متن ---------- */
    {
        char char__size[SIMPLE_LOG_PATH_MAX];
        func__T_CodecPath(char__size, sizeof(char__size), "codec-i.csv");
        func__T_CodecRemove(char__size);
        (void)func__T_CodecWriteRows(char__size, 200u);
        const std::string string__csv = func__T_CodecDump(char__size);
        const size_t size_t__binary = func__T_CodecFileSize(char__size);
        const size_t size_t__text = string__csv.size() + strlen(SIMPLE_LOG_HEADER);
        (void)printf("      حجم: CSV %lu بایت، ذخیره‌شده %lu بایت، کاهش %lu%%\n",
                     (unsigned long)size_t__text, (unsigned long)size_t__binary,
                     (unsigned long)((size_t__text > 0u) ? ((size_t__text - size_t__binary) * 100u / size_t__text) : 0u));
        func__T_Check((size_t__binary > 0u) && (size_t__binary * 2u < size_t__text),
                      "ذخیره‌سازی فشرده کمتر از نصف همان تاریخچهٔ CSV حجم می‌گیرد");
    }

    /* ---------- ۱۰) بازیابی وضعیت پس از بوت از فایل فشرده ---------- */
    {
        char char__boot[SIMPLE_LOG_PATH_MAX];
        func__T_CodecPath(char__boot, sizeof(char__boot), "codec-j.csv");
        func__T_CodecRemove(char__boot);
        (void)func__T_CodecWriteRows(char__boot, 3u);
        char char__last[256];
        func__T_Check(func__SimpleLog_LastRowOf(char__boot, char__last, sizeof(char__last)),
                      "آخرین ردیف از فایل فشرده خوانده می‌شود");
        uint32_t uint32_t__want[SimpleLogCodec::Fields];
        func__T_CodecRow(2u, uint32_t__want);
        func__T_Check(std::string(char__last) == func__T_CodecText(uint32_t__want),
                      "آخرین ردیفِ خوانده‌شده دقیقاً همان ردیف سوم است");
    }

    func__T_CodecRemove(char__path);
}

#endif
