/**
 * @file    up_settings.h
 * @brief   [EN] The panel's own settings that the OWNER may change: which
 *               engineering network this panel follows. A company that has more
 *               than one machine has more than one board, and each board brings
 *               its own access point; the admin keeps up to four of them here,
 *               one of which is active at a time, and switches between them
 *               from the panel's own page without re-flashing anything.
 *
 *          WHAT IS STORED
 *            A small table, `UP_F_NETWORK`, holding up to `UP_NET_PROFILE_MAX`
 *            entries of {SSID, password, when it was last used}. The file is
 *            protected by a magic word, a layout version and a CRC-32 over the
 *            whole structure: a half-written file - the one failure mode flash
 *            actually has - is detected and the defaults are used instead of a
 *            table full of garbage that would leave the panel unable to join
 *            anything.
 *
 *          WHAT IS NOT STORED HERE
 *            Nothing about the machine: samples, events, daily rollups and
 *            users live in their own files (up_store.h, up_auth.h). Nothing
 *            about Wi-Fi discipline either - this file only says WHERE to
 *            connect; up_link.h does the connecting.
 *
 *          PASSWORDS
 *            They are written to the panel's own flash because a station must
 *            have them to join. They are never sent back to a browser: the
 *            page learns only whether a password is set. That is a deliberate
 *            trade - anyone holding the panel can read flash - and the honest
 *            mitigation is that the panel's admin password is the gate to the
 *            only page that can change them.
 *
 * @brief   [FA] تنظیمات خودِ پنل که مالک می‌تواند عوض کند: این پنل کدام شبکهٔ
 *               مهندسی را دنبال می‌کند. شرکتی که چند ماشین دارد چند برد دارد و
 *               هر برد اکسس‌پوینت خودش را می‌آورد؛ مدیر تا چهار مورد از آن‌ها را
 *               همین‌جا نگه می‌دارد، هر بار یکی فعال است، و از صفحهٔ خود پنل بین
 *               آن‌ها جابه‌جا می‌شود، بدون فلش‌کردن دوباره.
 *
 *          چه چیزی ذخیره می‌شود
 *            یک جدول کوچک، `UP_F_NETWORK`، با حداکثر `UP_NET_PROFILE_MAX` ردیف
 *            شامل {نام شبکه، رمز، آخرین زمان استفاده}. فایل با یک کلمهٔ جادویی،
 *            یک شمارهٔ نسخهٔ چیدمان و یک CRC-32 روی کل ساختار محافظت می‌شود:
 *            فایل نیم‌نوشته - تنها خرابی‌ای که فلش واقعاً دارد - تشخیص داده
 *            می‌شود و به‌جای جدولی پر از آشغال که پنل را از اتصال به هر چیزی
 *            ناتوان می‌کند، مقادیر پیش‌فرض به کار می‌رود.
 *
 *          چه چیزی اینجا ذخیره نمی‌شود
 *            هیچ‌چیز دربارهٔ ماشین: نمونه‌ها، رویدادها، جمع‌های روزانه و کاربران
 *            فایل خودشان را دارند (up_store.h و up_auth.h). و هیچ‌چیز از نظم
 *            وای‌فای: این فایل فقط می‌گوید کجا وصل شود؛ وصل‌شدن کار up_link.h است.
 *
 *          رمزها
 *            در فلش خود پنل نوشته می‌شوند چون یک کلاینت برای پیوستن به شبکه
 *            ناچار به داشتنشان است. ولی هرگز به مرورگر برگردانده نمی‌شوند:
 *            صفحه فقط می‌فهمد رمزی تنظیم شده یا نه. این معاوضهٔ آگاهانه است -
 *            هر کس پنل را در دست بگیرد می‌تواند فلش را بخواند - و کاهش صادقانهٔ
 *            آن این است که گذرواژهٔ مدیر پنل تنها دروازهٔ صفحه‌ای است که می‌تواند
 *            آن‌ها را عوض کند.
 *
 *          LAYER / لایه
 *            9 of 11. Needs: config, flash (mounted by up_store.h), nothing
 *            else. Is needed by: up_link.h (where to connect) and up_http.h
 *            (the admin page that changes it). Its CRC is its own because this
 *            layer loads before the workbook writer, whose CRC-32 is one file
 *            later in the include order; fifteen copied lines are cheaper than
 *            a load-order knot.
 *            ۹ از ۱۱. نیاز دارد به: config و فلش (که up_store.h سوار می‌کند) و
 *            چیز دیگری نه. نیاز دارند به آن: up_link.h (کجا وصل شود) و
 *            up_http.h (صفحهٔ مدیری که عوضش می‌کند). CRC خودش را دارد چون این
 *            لایه پیش از نویسندهٔ کتاب اکسل بار می‌شود و CRC آن یک فایل بعدتر در
 *            ترتیب include است؛ پانزده خط تکراری از گره‌خوردن ترتیب ارزان‌تر است.
 */

#ifndef UP_SETTINGS_H
#define UP_SETTINGS_H

/**
 * [EN] The magic word is 'UPNW' read as it is written on disk (little-endian
 *      first byte 'U'): a file that starts with anything else is not ours.
 * [FA] کلمهٔ جادویی همان 'UPNW' است که روی دیسک نوشته می‌شود (کم‌ارزش‌ترین بایت
 *      اول، 'U'): فایلی که با چیز دیگری شروع شود مال ما نیست.
 */
#define UP_SETTINGS_MAGIC       0x574E5055u
#define UP_SETTINGS_VERSION     1u

/**
 * [EN] One saved board's network. The last-used stamp is what gets sacrificed
 *      when a fifth network arrives and the table is full: the stale one goes,
 *      never the active one.
 * [FA] شبکهٔ یک برد ذخیره‌شده. مهر آخرین استفاده همان چیزی است که وقتی شبکهٔ
 *      پنجم برسد و جدول پر باشد قربانی می‌شود: آن کهنه می‌رود، هرگز آن فعال.
 */
typedef struct
{
    char     char__ssid[UP_NET_SSID_MAX + 1u];
    char     char__pass[UP_NET_PASS_MAX + 1u];
    uint8_t  uint8_t__used;
    uint8_t  uint8_t__reserved[3];
    uint32_t uint32_t__lastUsedS;
} up_net_profile_t;

/**
 * [EN] The whole table as it sits in flash. The CRC covers every byte of this
 *      structure with the CRC field itself zeroed, so it also covers the magic
 *      and the version - there is no part of the file that can rot unnoticed.
 * [FA] کل جدول همان‌طور که در فلش می‌نشیند. CRC روی هر بایت این ساختار حساب
 *      می‌شود با فیلد CRC صفرشده، پس کلمهٔ جادویی و نسخه را هم پوشش می‌دهد -
 *      هیچ بخشی از فایل نیست که بی‌سروصدا بپوسد.
 */
typedef struct
{
    uint32_t         uint32_t__magic;
    uint32_t         uint32_t__version;
    uint32_t         uint32_t__crc;
    uint8_t          uint8_t__active;
    uint8_t          uint8_t__count;
    uint8_t          uint8_t__reserved[2];
    up_net_profile_t up_net_profile_t__items[UP_NET_PROFILE_MAX];
} up_net_table_t;

static up_net_table_t UP_NET_TABLE_T__G__Table;
static bool BOOL__G__UpSettingsFromFlash = false;

/* ==================== Names inside the table / عملیات ابتدایی ==================== */
/**
 * @brief  [EN] Is this SSID something a station can actually join? Empty and
 *              over-long are refused rather than truncated, because a
 *              truncated SSID is a network that does not exist.
 *         [FA] آیا این نام شبکه چیزی است که یک کلاینت واقعاً می‌تواند به آن
 *              بپیوندد؟ خالی و بیش‌ازحد بلند رد می‌شوند و نه بریده، چون نام
 *              بریده شبکه‌ای است که وجود ندارد.
 *              Printable characters only: the name is echoed inside a JSON
 *              reply and written to a file read by humans, and one quote or one
 *              control byte in an SSID would either break the reply or hide
 *              what is actually stored. Real access points do not use them, so
 *              refusing costs nothing and removes a whole class of surprise.
 *         [FA] فقط نویسه‌های چاپ‌شدنی: نام داخل یک پاسخ JSON بازتاب می‌شود و در
 *              فایلی نوشته می‌شود که آدم‌ها می‌خوانند، و یک کوتیشن یا یک بایت
 *              کنترلی در نام شبکه یا پاسخ را می‌شکند یا پنهان می‌کند که واقعاً
 *              چه ذخیره شده. اکسس‌پوینت واقعی از آن‌ها استفاده نمی‌کند، پس رد
 *              کردنشان هزینه‌ای ندارد و یک کلاس کامل از غافلگیری را برمی‌دارد.
 * @param  char__ssid [EN] candidate name / [FA] نام نامزد
 * @return [EN] true when the name may be stored / [FA] اگر نام قابل ذخیره باشد
 */
static bool func__UpSettings_SsidOk(const char *char__ssid)
{
    size_t size_t__len = strlen(char__ssid);

    if ((size_t__len == 0u) || (size_t__len > (size_t)UP_NET_SSID_MAX))
    {
        return false;
    }

    for (size_t size_t__i = 0u; size_t__i < size_t__len; size_t__i++)
    {
        unsigned char unsigned_char__ch = (unsigned char)char__ssid[size_t__i];

        if ((unsigned_char__ch < 0x20u) || (unsigned_char__ch == 0x7Fu))
        {
            return false;
        }
        if ((unsigned_char__ch == '"') || (unsigned_char__ch == '\\'))
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief  [EN] Is this password a legal WPA2 passphrase, or an empty one for an
 *              open network? Anything between one and seven characters is
 *              refused: it is the one length that can be typed by mistake and
 *              can never work.
 *         [FA] آیا این رمز یک عبارت عبور معتبر WPA2 است، یا خالی برای شبکهٔ باز؟
 *              هر چیزی بین یک تا هفت نویسه رد می‌شود: همان طولی است که ممکن است
 *              اشتباهی تایپ شود و هرگز کار نمی‌کند.
 * @param  char__pass [EN] candidate password / [FA] رمز نامزد
 * @return [EN] true when it may be stored / [FA] اگر قابل ذخیره باشد
 */
static bool func__UpSettings_PassOk(const char *char__pass)
{
    size_t size_t__len = strlen(char__pass);

    if (size_t__len == 0u)
    {
        return true;                     /* [EN] open network / [FA] شبکهٔ باز   */
    }
    if (size_t__len < 8u)
    {
        return false;
    }
    if (size_t__len > (size_t)UP_NET_PASS_MAX)
    {
        return false;
    }

    return true;
}

/**
 * @brief  [EN] Copy a bounded string into a fixed field. The caller has already
 *              validated the length; this refuses rather than truncates, so the
 *              two cannot disagree silently.
 *         [FA] کپی یک رشتهٔ کران‌دار در فیلدی با اندازهٔ ثابت. فراخوان طول را
 *              قبلاً بررسی کرده؛ این هم رد می‌کند و نمی‌بُرد، تا آن دو بی‌صدا با
 *              هم اختلاف پیدا نکنند.
 * @param  char__dst [EN] destination field / [FA] فیلد مقصد
 * @param  size_t__room [EN] bytes available / [FA] بایت‌های موجود
 * @param  char__src [EN] source text / [FA] متن مبدأ
 * @return [EN] true when it fitted / [FA] اگر جا شد
 */
static bool func__UpSettings_CopyField(char *char__dst, size_t size_t__room, const char *char__src)
{
    size_t size_t__len = strlen(char__src);

    if ((size_t__len + 1u) > size_t__room)
    {
        return false;
    }

    memcpy(char__dst, char__src, size_t__len + 1u);

    return true;
}

/* ==================== CRC / بازبینی ==================== */
/**
 * @brief  [EN] Move a CRC-32 (reflected, polynomial 0xEDB88320 - the one ZIP
 *              and every flash journal in this project already use) forward by
 *              a block of bytes. No table in flash: eight shifts per byte.
 *         [FA] جلو بردن CRC-32 (بازتابی، چندجمله‌ای 0xEDB88320 - همان که ZIP و
 *              هر ژورنال فلشی در این پروژه استفاده می‌کند) با یک بلوک بایت.
 *              بدون جدول در فلش: هشت شیفت برای هر بایت.
 * @param  uint32_t__crc [EN] running CRC / [FA] CRC جاری
 * @param  uint8_t__data [EN] bytes / [FA] بایت‌ها
 * @param  uint32_t__len [EN] how many / [FA] چند تا
 * @return [EN] the new CRC / [FA] CRC تازه
 */
static uint32_t func__UpSettings_Crc32(uint32_t uint32_t__crc, const uint8_t *uint8_t__data,
                                       uint32_t uint32_t__len)
{
    uint32_t uint32_t__state = uint32_t__crc;

    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__len; uint32_t__i++)
    {
        uint32_t__state ^= (uint32_t)uint8_t__data[uint32_t__i];

        for (uint8_t uint8_t__bit = 0u; uint8_t__bit < 8u; uint8_t__bit++)
        {
            uint32_t uint32_t__shifted = uint32_t__state >> 1;
            uint32_t uint32_t__mask = (uint32_t__state & 1u) ? 0xEDB88320u : 0u;

            uint32_t__state = uint32_t__shifted ^ uint32_t__mask;
        }
    }

    return uint32_t__state;
}

/**
 * @brief  [EN] The CRC of the table as it is right now, with the CRC field
 *              treated as zero so that storing the value cannot change it.
 *         [FA] CRC جدول در همین لحظه، با فیلد CRC صفر فرض‌شده تا نوشتن خودِ
 *              مقدار نتواند عوضش کند.
 * @return [EN] CRC-32 of the whole structure / [FA] CRC-32 کل ساختار
 */
static uint32_t func__UpSettings_TableCrc(void)
{
    uint32_t uint32_t__kept = UP_NET_TABLE_T__G__Table.uint32_t__crc;

    UP_NET_TABLE_T__G__Table.uint32_t__crc = 0u;

    uint32_t uint32_t__crc = func__UpSettings_Crc32(0u, (const uint8_t *)&UP_NET_TABLE_T__G__Table,
                                                    (uint32_t)sizeof(UP_NET_TABLE_T__G__Table));

    UP_NET_TABLE_T__G__Table.uint32_t__crc = uint32_t__kept;

    return uint32_t__crc;
}

/* ==================== Seed and flash / مقدار اولیه و فلش ==================== */
/**
 * @brief  [EN] Fill the table with the compile-time network from up_config.h, so
 *              a panel that has never been configured behaves exactly like the
 *              firmware that shipped before this table existed.
 *         [FA] پر کردن جدول با شبکهٔ زمان-کامپایل از up_config.h، تا پنلی که
 *              هرگز تنظیم نشده دقیقاً مثل فرم‌وری رفتار کند که پیش از وجود این
 *              جدول عرضه شده بود.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSettings_SeedDefault(void)
{
    memset(&UP_NET_TABLE_T__G__Table, 0, sizeof(UP_NET_TABLE_T__G__Table));

    UP_NET_TABLE_T__G__Table.uint32_t__magic = UP_SETTINGS_MAGIC;
    UP_NET_TABLE_T__G__Table.uint32_t__version = UP_SETTINGS_VERSION;
    UP_NET_TABLE_T__G__Table.uint8_t__count = 1u;
    UP_NET_TABLE_T__G__Table.uint8_t__active = 0u;

    up_net_profile_t *up_net_profile_t__slot = &UP_NET_TABLE_T__G__Table.up_net_profile_t__items[0];

    up_net_profile_t__slot->uint8_t__used = 1u;
    (void)func__UpSettings_CopyField(up_net_profile_t__slot->char__ssid,
                                     sizeof(up_net_profile_t__slot->char__ssid), UP_STA_SSID);
    (void)func__UpSettings_CopyField(up_net_profile_t__slot->char__pass,
                                     sizeof(up_net_profile_t__slot->char__pass), UP_STA_PASS);
}

/**
 * @brief  [EN] Write the whole table to flash with a fresh CRC. One small file,
 *              written whole, is the safest thing flash can do: there is no
 *              update in place and therefore no state in which half of an old
 *              table and half of a new one are both live.
 *         [FA] نوشتن کل جدول در فلش با یک CRC تازه. یک فایل کوچک که یک‌جا
 *              نوشته می‌شود ایمن‌ترین کاری است که فلش می‌تواند بکند: به‌روزرسانی
 *              درجا وجود ندارد و پس از آن حالتی نیست که نیمی از جدول قدیم و
 *              نیمی از جدول تازه هر دو زنده باشند.
 * @return [EN] true when the bytes reached the flash / [FA] اگر بایت‌ها به فلش رسیدند
 */
static bool func__UpSettings_Save(void)
{
    UP_NET_TABLE_T__G__Table.uint32_t__crc = func__UpSettings_TableCrc();

    File up_file_t__file = LittleFS.open(UP_F_NETWORK, "w");

    if (!up_file_t__file)
    {
        return false;
    }

    size_t size_t__written = up_file_t__file.write((const uint8_t *)&UP_NET_TABLE_T__G__Table,
                                                   sizeof(UP_NET_TABLE_T__G__Table));

    up_file_t__file.close();

    return (size_t__written == sizeof(UP_NET_TABLE_T__G__Table));
}

/**
 * @brief  [EN] Read the table back and decide whether to believe it: the size
 *              must match this build's structure, the magic and version must be
 *              ours, and the CRC must cover the bytes that arrived.
 *         [FA] خواندن جدول و تصمیم دربارهٔ باورکردنش: اندازه باید با ساختار همین
 *              بیلد بخواند، کلمهٔ جادویی و نسخه باید مال ما باشند و CRC باید
 *              بایت‌های رسیده را پوشش دهد.
 * @return [EN] true when the file was accepted / [FA] اگر فایل پذیرفته شد
 */
static bool func__UpSettings_Load(void)
{
    File up_file_t__file = LittleFS.open(UP_F_NETWORK, "r");

    if (!up_file_t__file)
    {
        return false;
    }

    size_t size_t__got = up_file_t__file.read((uint8_t *)&UP_NET_TABLE_T__G__Table,
                                              sizeof(UP_NET_TABLE_T__G__Table));

    up_file_t__file.close();

    if (size_t__got != sizeof(UP_NET_TABLE_T__G__Table))
    {
        return false;
    }
    if (UP_NET_TABLE_T__G__Table.uint32_t__magic != UP_SETTINGS_MAGIC)
    {
        return false;
    }
    if (UP_NET_TABLE_T__G__Table.uint32_t__version != UP_SETTINGS_VERSION)
    {
        return false;
    }
    if (UP_NET_TABLE_T__G__Table.uint32_t__crc != func__UpSettings_TableCrc())
    {
        return false;
    }
    if (UP_NET_TABLE_T__G__Table.uint8_t__active >= UP_NET_PROFILE_MAX)
    {
        return false;
    }
    if (UP_NET_TABLE_T__G__Table.up_net_profile_t__items[UP_NET_TABLE_T__G__Table.uint8_t__active]
            .uint8_t__used == 0u)
    {
        return false;                     /* [EN] active slot is empty / [FA] اسلات فعال خالی است */
    }

    return true;
}

/**
 * @brief  [EN] Bring the settings layer up after the flash is mounted. A missing
 *              or damaged file is not an error the owner should ever see: the
 *              defaults are written and the panel joins the engineering network
 *              it was built for.
 *         [FA] بالا آوردن لایهٔ تنظیمات پس از سوار شدن فلش. فایل نبودن یا خراب
 *              بودن خطایی نیست که مالک باید ببیند: مقادیر پیش‌فرض نوشته می‌شوند و
 *              پنل به همان شبکهٔ مهندسی‌ای می‌پیوندد که برایش ساخته شده.
 * @return [EN] true when a stored table was used / [FA] اگر جدول ذخیره‌شده به کار رفت
 */
static bool func__UpSettings_Begin(void)
{
    if (func__UpSettings_Load())
    {
        BOOL__G__UpSettingsFromFlash = true;
        return true;
    }

    func__UpSettings_SeedDefault();
    (void)func__UpSettings_Save();
    BOOL__G__UpSettingsFromFlash = false;

    return false;
}

/* ==================== The table, read / خواندن جدول ==================== */
/**
 * @brief  [EN] The profile that is active right now. Never returns an empty
 *              slot: Begin() refuses to accept a table whose active entry is
 *              empty, and Delete() refuses to remove it.
 *         [FA] پروفایلی که همین حالا فعال است. هرگز اسلات خالی برنمی‌گرداند:
 *              Begin() جدولی را که ردیف فعالش خالی است نمی‌پذیرد و Delete()
 *              حذفش را رد می‌کند.
 * @return [EN] pointer into the table / [FA] اشاره‌گر به داخل جدول
 */
static up_net_profile_t *func__UpSettings_Active(void)
{
    return &UP_NET_TABLE_T__G__Table.up_net_profile_t__items[UP_NET_TABLE_T__G__Table.uint8_t__active];
}

/**
 * @brief  [EN] One row of the table by index, or NULL when the index is out of
 *              range or that row was never used.
 *         [FA] یک ردیف جدول با شماره، یا NULL اگر شماره بیرون از بازه باشد یا
 *              آن ردیف هرگز استفاده نشده باشد.
 * @param  uint8_t__index [EN] 0..UP_NET_PROFILE_MAX-1 / [FA] ۰ تا ۳
 * @return [EN] pointer or NULL / [FA] اشاره‌گر یا NULL
 */
static up_net_profile_t *func__UpSettings_Profile(uint8_t uint8_t__index)
{
    if (uint8_t__index >= UP_NET_PROFILE_MAX)
    {
        return NULL;
    }

    up_net_profile_t *up_net_profile_t__row = &UP_NET_TABLE_T__G__Table.up_net_profile_t__items[uint8_t__index];

    if (up_net_profile_t__row->uint8_t__used == 0u)
    {
        return NULL;
    }

    return up_net_profile_t__row;
}

/**
 * @brief  [EN] Which row is active, and how many rows are in use.
 *         [FA] کدام ردیف فعال است و چند ردیف در استفاده است.
 * @return [EN] the active index / [FA] شمارهٔ ردیف فعال
 */
static uint8_t func__UpSettings_ActiveIndex(void)
{
    return UP_NET_TABLE_T__G__Table.uint8_t__active;
}

/**
 * @brief  [EN] How many rows are in use. The count field is maintained by the
 *              add and delete paths, but it is recomputed here from the used
 *              flags: the flags are the truth, the counter is a convenience.
 *         [FA] چند ردیف در استفاده است. فیلد شمارنده را مسیرهای افزودن و حذف
 *              نگه می‌دارند، ولی اینجا از پرچم‌های used بازشماری می‌شود:
 *              پرچم‌ها حقیقت‌اند و شمارنده فقط یک راحتی.
 * @return [EN] used rows / [FA] ردیف‌های استفاده‌شده
 */
static uint8_t func__UpSettings_Count(void)
{
    uint8_t uint8_t__used = 0u;

    for (uint8_t uint8_t__i = 0u; uint8_t__i < UP_NET_PROFILE_MAX; uint8_t__i++)
    {
        if (UP_NET_TABLE_T__G__Table.up_net_profile_t__items[uint8_t__i].uint8_t__used != 0u)
        {
            uint8_t__used++;
        }
    }

    UP_NET_TABLE_T__G__Table.uint8_t__count = uint8_t__used;

    return uint8_t__used;
}

/**
 * @brief  [EN] Find the row holding this SSID, or -1. Case-sensitive on
 *              purpose: an access point's name is a byte string, and the
 *              ESP8266's own Wi-Fi stack is case-sensitive too, so pretending
 *              otherwise here would create a setting that looks saved and never
 *              connects.
 *         [FA] پیدا کردن ردیفی که این نام شبکه را دارد، یا ۱-. حساس به بزرگی و
 *              کوچکی حروف عمدی است: نام اکسس‌پوینت یک رشتهٔ بایتی است و پشتهٔ
 *              وای‌فای خود ESP8266 هم حساس است، پس رفتار دیگری اینجا تنظیمی
 *              می‌سازد که ذخیره به‌نظر می‌رسد و هرگز وصل نمی‌شود.
 * @param  char__ssid [EN] network name / [FA] نام شبکه
 * @return [EN] index or -1 / [FA] شماره یا ۱-
 */
static int8_t func__UpSettings_Find(const char *char__ssid)
{
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UP_NET_PROFILE_MAX; uint8_t__i++)
    {
        up_net_profile_t *up_net_profile_t__row = &UP_NET_TABLE_T__G__Table.up_net_profile_t__items[uint8_t__i];

        if (up_net_profile_t__row->uint8_t__used == 0u)
        {
            continue;
        }
        if (strcmp(up_net_profile_t__row->char__ssid, char__ssid) == 0)
        {
            return (int8_t)uint8_t__i;
        }
    }

    return -1;
}

/* ==================== The table, written / نوشتن جدول ==================== */
/**
 * @brief  [EN] The first empty row, or -1 when the table is full.
 *         [FA] اولین ردیف خالی، یا ۱- وقتی جدول پر است.
 * @return [EN] index or -1 / [FA] شماره یا ۱-
 */
static int8_t func__UpSettings_FreeRow(void)
{
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UP_NET_PROFILE_MAX; uint8_t__i++)
    {
        if (UP_NET_TABLE_T__G__Table.up_net_profile_t__items[uint8_t__i].uint8_t__used == 0u)
        {
            return (int8_t)uint8_t__i;
        }
    }

    return -1;
}

/**
 * @brief  [EN] The least recently used row that is NOT active - the one that
 *              gets overwritten when a fifth network arrives. Returns -1 if the
 *              only used rows are the active one and rows with no stamp, which
 *              cannot happen while four rows exist and one of them is active,
 *              but is checked anyway because "cannot happen" is not a proof.
 *         [FA] کم‌استفاده‌ترین ردیفی که فعال نیست - همان که وقتی شبکهٔ پنجم برسد
 *              بازنویسی می‌شود. اگر تنها ردیف‌های استفاده‌شده ردیف فعال و
 *              ردیف‌های بی‌مهر باشند ۱- برمی‌گرداند، که تا وقتی چهار ردیف هست و
 *              یکی فعال است نمی‌تواند رخ دهد، ولی بررسی می‌شود چون «نمی‌تواند رخ
 *              دهد» اثبات نیست.
 * @return [EN] index or -1 / [FA] شماره یا ۱-
 */
static int8_t func__UpSettings_StalestRow(void)
{
    int8_t int8_t__best = -1;

    for (uint8_t uint8_t__i = 0u; uint8_t__i < UP_NET_PROFILE_MAX; uint8_t__i++)
    {
        up_net_profile_t *up_net_profile_t__row = &UP_NET_TABLE_T__G__Table.up_net_profile_t__items[uint8_t__i];

        if (up_net_profile_t__row->uint8_t__used == 0u)
        {
            continue;
        }
        if (uint8_t__i == UP_NET_TABLE_T__G__Table.uint8_t__active)
        {
            continue;
        }
        if (int8_t__best < 0)
        {
            int8_t__best = (int8_t)uint8_t__i;
            continue;
        }
        if (up_net_profile_t__row->uint32_t__lastUsedS <
            UP_NET_TABLE_T__G__Table.up_net_profile_t__items[(uint8_t)int8_t__best].uint32_t__lastUsedS)
        {
            int8_t__best = (int8_t)uint8_t__i;
        }
    }

    return int8_t__best;
}

/**
 * @brief  [EN] Save a network. An existing SSID is updated in place (so its
 *              position and stamp survive); a new one takes a free row, or the
 *              stalest row when the table is full. Nothing is written to flash
 *              until the caller decides to switch or the save path runs.
 *         [FA] ذخیرهٔ یک شبکه. نام موجود درجایش به‌روز می‌شود (تا جایگاه و مهرش
 *              بماند)؛ نام تازه یک ردیف خالی می‌گیرد، یا وقتی جدول پر است کهنه‌ترین
 *              ردیف را. تا وقتی فراخوان تصمیم به جابه‌جایی نگیرد یا مسیر ذخیره
 *              اجرا شود چیزی در فلش نوشته نمی‌شود.
 * @param  char__ssid [EN] validated name / [FA] نام بررسی‌شده
 * @param  char__pass [EN] validated password / [FA] رمز بررسی‌شده
 * @param  bool__keepPass [EN] true = keep the stored password / [FA] درست = رمز ذخیره‌شده بماند
 * @return [EN] row index or -1 / [FA] شمارهٔ ردیف یا ۱-
 */
static int8_t func__UpSettings_Put(const char *char__ssid, const char *char__pass, bool bool__keepPass)
{
    int8_t int8_t__index = func__UpSettings_Find(char__ssid);

    if (int8_t__index < 0)
    {
        int8_t__index = func__UpSettings_FreeRow();

        if (int8_t__index < 0)
        {
            int8_t__index = func__UpSettings_StalestRow();

            if (int8_t__index < 0)
            {
                return -1;
            }
        }

        up_net_profile_t *up_net_profile_t__fresh =
            &UP_NET_TABLE_T__G__Table.up_net_profile_t__items[(uint8_t)int8_t__index];

        memset(up_net_profile_t__fresh, 0, sizeof(*up_net_profile_t__fresh));
        up_net_profile_t__fresh->uint8_t__used = 1u;
    }

    up_net_profile_t *up_net_profile_t__row =
        &UP_NET_TABLE_T__G__Table.up_net_profile_t__items[(uint8_t)int8_t__index];

    if (!func__UpSettings_CopyField(up_net_profile_t__row->char__ssid,
                                    sizeof(up_net_profile_t__row->char__ssid), char__ssid))
    {
        return -1;
    }

    if (!bool__keepPass)
    {
        if (!func__UpSettings_CopyField(up_net_profile_t__row->char__pass,
                                        sizeof(up_net_profile_t__row->char__pass), char__pass))
        {
            return -1;
        }
    }

    (void)func__UpSettings_Count();

    return int8_t__index;
}

/**
 * @brief  [EN] Make a row active and write the table. The caller reconnects the
 *              Wi-Fi afterwards; this function deliberately knows nothing about
 *              radios, so it can be tested without one.
 *         [FA] یک ردیف را فعال کردن و نوشتن جدول. فراخوان بعدش وای‌فای را دوباره
 *              وصل می‌کند؛ این تابع عمداً از رادیو چیزی نمی‌داند تا بدون رادیو هم
 *              قابل آزمایش باشد.
 * @param  uint8_t__index [EN] row to activate / [FA] ردیفی که فعال شود
 * @return [EN] true when it is now active and stored / [FA] اگر حالا فعال و ذخیره باشد
 */
static bool func__UpSettings_SetActive(uint8_t uint8_t__index)
{
    if (func__UpSettings_Profile(uint8_t__index) == NULL)
    {
        return false;
    }

    UP_NET_TABLE_T__G__Table.uint8_t__active = uint8_t__index;

    return func__UpSettings_Save();
}

/**
 * @brief  [EN] Remove a row. The active row cannot be removed: the panel must
 *              always know where it is trying to connect, and a table whose
 *              active entry is gone is exactly the state that turns a
 *              mis-tap into a panel that needs a serial cable.
 *         [FA] حذف یک ردیف. ردیف فعال حذف‌شدنی نیست: پنل باید همیشه بداند کجا
 *              می‌خواهد وصل شود، و جدولی که ردیف فعالش رفته دقیقاً همان حالتی است
 *              که یک اشتباه کوچک را به پنلی تبدیل می‌کند که کابل سریال می‌خواهد.
 * @param  uint8_t__index [EN] row to remove / [FA] ردیفی که حذف شود
 * @return [EN] true when it was removed and stored / [FA] اگر حذف و ذخیره شد
 */
static bool func__UpSettings_Delete(uint8_t uint8_t__index)
{
    up_net_profile_t *up_net_profile_t__row = func__UpSettings_Profile(uint8_t__index);

    if (up_net_profile_t__row == NULL)
    {
        return false;
    }
    if (uint8_t__index == UP_NET_TABLE_T__G__Table.uint8_t__active)
    {
        return false;
    }

    memset(up_net_profile_t__row, 0, sizeof(*up_net_profile_t__row));
    (void)func__UpSettings_Count();

    return func__UpSettings_Save();
}

/**
 * @brief  [EN] Remember WHEN a network was last used, so the next full table
 *              sacrifices the one nobody has touched. Written at most once an
 *              hour per row: a flash cell that is rewritten every few seconds
 *              by a panel that is simply doing its job is how flash dies young.
 *         [FA] یادداشت اینکه یک شبکه آخرین بار کِی استفاده شد، تا جدول پر
 *              دفعهٔ بعد آن را قربانی کند که کسی دستش به آن نخورده. حداکثر
 *              ساعتی یک‌بار برای هر ردیف نوشته می‌شود: سلول فلشی که پنلی در حال
 *              کار عادی هر چند ثانیه بازنویسی‌اش کند، همان‌طور جوان می‌میرد.
 * @param  uint8_t__index [EN] row just used / [FA] ردیفی که همین حالا استفاده شد
 * @param  uint32_t__nowS [EN] seconds since the epoch, 0 if unknown / [FA] ثانیه از مبدأ، صفر اگر نامعلوم
 * @return [EN] true when the stamp was written / [FA] اگر مهر نوشته شد
 */
static bool func__UpSettings_NoteUsed(uint8_t uint8_t__index, uint32_t uint32_t__nowS)
{
    if (uint32_t__nowS == 0u)
    {
        return false;
    }

    up_net_profile_t *up_net_profile_t__row = func__UpSettings_Profile(uint8_t__index);

    if (up_net_profile_t__row == NULL)
    {
        return false;
    }

    uint32_t uint32_t__age = uint32_t__nowS - up_net_profile_t__row->uint32_t__lastUsedS;

    if ((up_net_profile_t__row->uint32_t__lastUsedS != 0u) && (uint32_t__age < 3600u))
    {
        return false;
    }

    up_net_profile_t__row->uint32_t__lastUsedS = uint32_t__nowS;

    return func__UpSettings_Save();
}

#endif /* UP_SETTINGS_H */
